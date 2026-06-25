/**
 * crsf.c — CRSF Half-Duplex STM32F103C8T6
 */

#include "crsf.h"
#include "debug.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── Variáveis internas ─────────────────────────────────────────── */
static UART_HandleTypeDef *_huart;

/* Controle — volatile, escrito na interrupção USB */
static volatile int16_t g_direcao  = 1500;
static volatile int16_t g_throttle = 1500;

/* ACK — montado na interrupção, enviado pela task */
static char             g_ack_buf[64];
static volatile uint8_t g_ack_len     = 0;
static volatile bool    g_ack_pending = false;

/* Handle da CRSF_task — usado para notificar fim de DMA a partir da ISR */
static TaskHandle_t s_crsf_task = NULL;

/* Failsafe — tick e sequência da última recepção USB válida */
static volatile uint32_t g_last_rx_tick = 0;
static volatile int32_t  g_last_seq     = -1;

/* ─── Recepção de telemetria (RX) ───────────────────────────────── */
#define CRSF_RX_DEBUG    1     /* 1 = loga frames recebidos na USART2 */
#define CRSF_RX_HEXDUMP  0     /* 1 = inclui os bytes em hexadecimal   */
#define RX_RING_SZ       256   /* potência de 2; preenchido na ISR     */

static volatile uint8_t  rx_ring[RX_RING_SZ];
static volatile uint16_t rx_head = 0;   /* escrito na ISR */
static uint16_t          rx_tail = 0;   /* lido na task   */

/* Parser de frames */
typedef enum { RXS_SYNC = 0, RXS_LEN, RXS_DATA } rx_state_t;
static rx_state_t rx_state = RXS_SYNC;
static uint8_t    rx_frame[CRSF_MAX_PACKET_SIZE];
static uint8_t    rx_flen  = 0;   /* valor do byte de length */
static uint8_t    rx_fidx  = 0;   /* bytes já acumulados     */

/* ─── CRC8 ───────────────────────────────────────────────────────── */
static uint8_t crsf_crc8(const uint8_t *buf, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0xD5 : (crc << 1);
    }
    return crc;
}

/* ─── Conversores ────────────────────────────────────────────────── */
uint16_t crsf_from_us(int16_t us)
{
    if (us > 2000) us = 2000;
    if (us < 1000) us = 1000;
    if (us >= 1500)
        return CRSF_CH_MID + ((int32_t)(us-1500)*(CRSF_CH_MAX-CRSF_CH_MID))/500;
    else
        return CRSF_CH_MID - ((int32_t)(1500-us)*(CRSF_CH_MID-CRSF_CH_MIN))/500;
}

uint16_t crsf_from_us_dir(int16_t us) { return crsf_from_us(us); }

/* ─── Parse JSON ─────────────────────────────────────────────────── */
static int json_get_int(const char *json, const char *key, int def)
{
    char pat[32];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p == ' ') p++;
    if (*p == '-' || (*p >= '0' && *p <= '9'))
        return (int)strtol(p, NULL, 10);
    return def;
}

/* ─── USB receive — chamado na interrupção, NÃO bloquear ────────── */
void crsf_usb_receive(const char *json)
{
    int dir = json_get_int(json, "direcao",  1500);
    int thr = json_get_int(json, "throttle", 1500);
    int seq = json_get_int(json, "seq",      0);

    if (dir < 1000) dir = 1000; if (dir > 2000) dir = 2000;
    if (thr < 1000) thr = 1000; if (thr > 2000) thr = 2000;

    //DBG_FMT("[USB] dir=%d thr=%d seq=%d\r\n", dir, thr, seq);

    /* Atualiza volatile — sem mutex, seguro em interrupção */
    g_direcao      = (int16_t)dir;
    g_throttle     = (int16_t)thr;
    g_last_rx_tick = HAL_GetTick();
    g_last_seq     = (int32_t)seq;

    /* Prepara ACK simples */
    if (!g_ack_pending) {
        int ack_result = snprintf(g_ack_buf, sizeof(g_ack_buf),
            "{\"direcao\":%d,\"throttle\":%d,\"seq\":%d,\"ok\":1}\n", dir, thr, seq);
        if (ack_result > 0 && ack_result < (int)sizeof(g_ack_buf)) {
            g_ack_len = (uint8_t)ack_result;
            g_ack_pending = true;
        }
    }
}

/* ─── Callback TX CRSF — fim do DMA, contexto de interrupção ─────── */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && s_crsf_task != NULL) {
        BaseType_t higher_woken = pdFALSE;
        vTaskNotifyGiveFromISR(s_crsf_task, &higher_woken);
        portYIELD_FROM_ISR(higher_woken);
    }
}

/* ─── Inicialização — chamado dentro da CRSF_task ────────────────── */
void crsf_init(UART_HandleTypeDef *huart)
{
    _huart      = huart;
    s_crsf_task = xTaskGetCurrentTaskHandle();   /* p/ notificação de fim de DMA */
    DBG_FMT("[CRSF] Init OK — %dHz\r\n", CRSF_RATE_HZ);
    HAL_HalfDuplex_EnableReceiver(_huart);
    /* Habilita interrupção RXNE p/ telemetria (NVIC USART1 já ativo no MSP).
     * A própria ISR (crsf_uart_irq_handler) lê DR antes da HAL, que vira no-op. */
    __HAL_UART_ENABLE_IT(_huart, UART_IT_RXNE);
}

/* ─── Envio RC Channels ──────────────────────────────────────────── */
void crsf_send_channels(const uint16_t ch[16])
{
    uint8_t buf[CRSF_RC_FRAME_SIZE] = {0};
    buf[0] = CRSF_SYNC_BYTE;
    buf[1] = 0x18;
    buf[2] = CRSF_TYPE_RC_CHANNELS;

    uint32_t bits = 0; uint8_t avail = 0; uint8_t idx = 0;
    for (int i = 0; i < 16; i++) {
        uint16_t val = ch[i];
        if (val < CRSF_CH_MIN) val = CRSF_CH_MIN;
        if (val > CRSF_CH_MAX) val = CRSF_CH_MAX;
        bits  |= ((uint32_t)(val & 0x7FF)) << avail;
        avail += 11;
        while (avail >= 8) { buf[3+idx++]=bits&0xFF; bits>>=8; avail-=8; }
    }
    configASSERT(idx == 22); /* 16 ch × 11 bits = 176 bits = 22 bytes */
    buf[25] = crsf_crc8(&buf[2], 23);

    /* Limpa notificação pendente antes de iniciar o DMA */
    ulTaskNotifyTake(pdTRUE, 0);
    HAL_HalfDuplex_EnableTransmitter(_huart);
    HAL_UART_Transmit_DMA(_huart, buf, CRSF_RC_FRAME_SIZE);
    /* Bloqueia (sem busy-wait) até o TxCpltCallback notificar; timeout 2 ms */
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
    HAL_HalfDuplex_EnableReceiver(_huart);
}

/* ─── ISR de RX — chamada por USART1_IRQHandler (stm32f1xx_it.c) ──
 * Lê o byte em nível de registrador (limpa RXNE e flags de erro) e o
 * empilha no ring buffer. Não usa a máquina de estados da HAL, evitando
 * conflito com o TX por DMA / half-duplex. Roda em contexto de interrupção.
 */
void crsf_uart_irq_handler(void)
{
    uint32_t sr = USART1->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        uint8_t b = (uint8_t)(USART1->DR);   /* leitura limpa RXNE + erros */
        if (sr & USART_SR_RXNE) {
            uint16_t next = (uint16_t)((rx_head + 1u) & (RX_RING_SZ - 1));
            if (next != rx_tail) {           /* descarta se buffer cheio */
                rx_ring[rx_head] = b;
                rx_head = next;
            }
        }
    }
}

/* ─── Log de um frame recebido (USART2) ─────────