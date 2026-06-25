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
#define CRSF_RX_DEBUG        1   /* 1 = loga telemetria na USART2          */
#define CRSF_RX_DUMP_OTHERS  0   /* 1 = loga [RX] genérico p/ tipos s/ decode */
#define CRSF_RX_HEXDUMP      0   /* 1 = inclui os bytes em hexadecimal     */
#define RX_RING_SZ           256 /* potência de 2; preenchido na ISR       */

static volatile uint8_t  rx_ring[RX_RING_SZ];
static volatile uint16_t rx_head = 0;   /* escrito na ISR */
static uint16_t          rx_tail = 0;   /* lido na task   */

/* Última telemetria de Link Statistics (0x14) decodificada — pronta p/ USB */
typedef struct {
    int8_t   up_rssi1, up_rssi2;  /* dBm (int8 neste módulo) */
    uint8_t  up_lq;               /* link quality uplink (%) */
    int8_t   up_snr;              /* dB                      */
    uint8_t  active_ant;
    uint8_t  rf_mode;             /* índice de modo RF (TBS) */
    uint16_t up_power_mw;         /* potência de TX (mW)     */
    int8_t   down_rssi;           /* dBm                     */
    uint8_t  down_lq;             /* link quality downlink (%) */
    int8_t   down_snr;            /* dB                      */
    bool     valid;
} crsf_link_t;
static crsf_link_t g_link;

/* Última telemetria de bateria (0x08) */
typedef struct {
    int16_t  voltage_dv;   /* 0,1 V/LSB */
    int16_t  current_da;   /* 0,1 A/LSB */
    uint32_t capacity_mah;
    uint8_t  remaining;    /* %         */
    bool     valid;
} crsf_batt_t;
static crsf_batt_t g_batt;

/* Última correção de timing (0x3A.0x10, header estendido) */
typedef struct {
    uint32_t interval_100ns;  /* update_interval (100 ns/LSB) */
    int32_t  offset_100ns;    /* offset (100 ns/LSB)          */
    bool     valid;
} crsf_timing_t;
static crsf_timing_t g_timing;

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

/* Tabela de potência (enum up_rf_power → mW), conforme spec CRSF */
static const uint16_t crsf_power_mw[] = { 0, 10, 25, 100, 500, 1000, 2000, 250, 50 };

/* ─── Decodifica 0x14 Link Statistics e loga na USART2 ──────────── */
static void crsf_decode_link(const uint8_t *p, uint8_t plen)
{
    if (plen < 10) return;                         /* payload mínimo */

    g_link.up_rssi1    = (int8_t)p[0];             /* RSSI é int8 neste módulo */
    g_link.up_rssi2    = (int8_t)p[1];
    g_link.up_lq       = p[2];
    g_link.up_snr      = (int8_t)p[3];
    g_link.active_ant  = p[4];
    g_link.rf_mode     = p[5];
    g_link.up_power_mw = (p[6] < (sizeof(crsf_power_mw)/sizeof(crsf_power_mw[0])))
                         ? crsf_power_mw[p[6]] : 0;
    g_link.down_rssi   = (int8_t)p[7];
    g_link.down_lq     = p[8];
    g_link.down_snr    = (int8_t)p[9];
    g_link.valid       = true;

#if CRSF_RX_DEBUG
    DBG_FMT("[LINK] LQ up=%u%% dn=%u%% | RSSI a1=%d a2=%d dn=%d dBm | SNR up=%d dn=%d dB | ant%u pwr=%umW mode%u\r\n",
            g_link.up_lq, g_link.down_lq,
            g_link.up_rssi1, g_link.up_rssi2, g_link.down_rssi,
            g_link.up_snr, g_link.down_snr,
            g_link.active_ant, g_link.up_power_mw, g_link.rf_mode);
#endif
}

/* ─── Decodifica 0x08 Battery Sensor e loga na USART2 ───────────── */
static void crsf_decode_battery(const uint8_t *p, uint8_t plen)
{
    if (plen < 8) return;
    g_batt.voltage_dv   = (int16_t)(((uint16_t)p[0] << 8) | p[1]);   /* big-endian */
    g_batt.current_da   = (int16_t)(((uint16_t)p[2] << 8) | p[3]);
    g_batt.capacity_mah = ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 8) | p[6];
    g_batt.remaining    = p[7];
    g_batt.valid        = true;

#if CRSF_RX_DEBUG
    DBG_FMT("[BATT] %d.%dV %d.%dA %umAh %u%%\r\n",
            g_batt.voltage_dv / 10, abs(g_batt.voltage_dv % 10),
            g_batt.current_da / 10, abs(g_batt.current_da % 10),
            (unsigned)g_batt.capacity_mah, g_batt.remaining);
#endif
}

/* ─── Decodifica 0x3A.0x10 Timing Correction (header estendido) ──
 * payload[] inclui dest, origin e subcmd antes dos dados:
 *   p[0]=dest(0xEA) p[1]=origin(0xEE) p[2]=subcmd(0x10)
 *   p[3..6]=interval (u32 BE)  p[7..10]=offset (i32 BE)
 */
static void crsf_decode_timing(const uint8_t *p, uint8_t plen)
{
    if (plen < 11 || p[2] != 0x10) return;          /* só Timing Correction */

    uint32_t interval = ((uint32_t)p[3] << 24) | ((uint32_t)p[4] << 16) |
                        ((uint32_t)p[5] << 8)  |  (uint32_t)p[6];
    int32_t  offset   = (int32_t)(((uint32_t)p[7] << 24) | ((uint32_t)p[8] << 16) |
                                  ((uint32_t)p[9] << 8)  |  (uint32_t)p[10]);
    g_timing.interval_100ns = interval;
    g_timing.offset_100ns   = offset;
    g_timing.valid          = true;

#if CRSF_RX_DEBUG
    /* 100 ns/LSB → µs com 1 casa; freq = 1e7 / interval */
    DBG_FMT("[TIME] interval=%u.%u us (%u Hz) offset=%d.%d us\r\n",
            (unsigned)(interval / 10), (unsigned)(interval % 10),
            (unsigned)(interval ? 10000000UL / interval : 0),
            (int)(offset / 10), (int)(abs((int)(offset % 10))));
#endif
}

/* ─── Despacha um frame válido por tipo ─────────────────────────── */
static void crsf_rx_dispatch(uint8_t type, const uint8_t *payload, uint8_t plen)
{
    switch (type) {
    case CRSF_TYPE_LINK_STATS:
        crsf_decode_link(payload, plen);
        break;
    case CRSF_TYPE_BATTERY:
        crsf_decode_battery(payload, plen);
        break;
    case CRSF_TYPE_RADIO_ID:
        crsf_decode_timing(payload, plen);
        break;
    default:
#if CRSF_RX_DEBUG && CRSF_RX_DUMP_OTHERS
        DBG_FMT("[RX] type=0x%02X len=%u crc=OK\r\n", type, (unsigned)(plen + 2));
#endif
        (void)payload; (void)plen;
        break;
    }
}

/* ─── Máquina de estados: processa 1 byte recebido ──────────────── */
static void crsf_rx_parse(uint8_t b)
{
    switch (rx_state) {
    case RXS_SYNC:                      /* aceita qualquer byte como sync */
        rx_frame[0] = b; rx_fidx = 1; rx_state = RXS_LEN;
        break;

    case RXS_LEN:
        if (b < 2 || b > 62) {          /* length fora da faixa → ressincroniza */
            rx_state = RXS_SYNC; rx_fidx = 0; break;
        }
        rx_frame[1] = b; rx_flen = b; rx_fidx = 2; rx_state = RXS_DATA;
        break;

    case RXS_DATA:
        rx_frame[rx_fidx++] = b;
        if (rx_fidx >= (uint8_t)(rx_flen + 2)) {        /* frame completo */
            uint8_t type    = rx_frame[2];
            uint8_t crc_rx  = rx_frame[rx_flen + 1];
            uint8_t crc_cal = crsf_crc8(&rx_frame[2], (uint8_t)(rx_flen - 1));
            if (crc_cal == crc_rx) {
                crsf_rx_dispatch(type, &rx_frame[3], (uint8_t)(rx_flen - 2));
            }
#if CRSF_RX_DEBUG
            else {
                DBG_FMT("[RX] type=0x%02X len=%u crc=ERR\r\n", type, rx_flen);
            }
#endif
            rx_state = RXS_SYNC; rx_fidx = 0;
        } else if (rx_fidx >= sizeof(rx_frame)) {       /* guarda de segurança */
            rx_state = RXS_SYNC; rx_fidx = 0;
        }
        break;
    }
}

/* ─── Drena o ring buffer e parseia (chamado no laço, fora da ISR) ─ */
static void crsf_rx_poll(void)
{
    static uint8_t stale = 0;
    bool consumed = false;

    while (rx_tail != rx_head) {
        uint8_t b = rx_ring[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1u) & (RX_RING_SZ - 1));
        crsf_rx_parse(b);
        consumed = true;
    }

    /* Anti-travamento: frame parcial sem novos bytes por alguns ciclos */
    if (!consumed && rx_state != RXS_SYNC) {
        if (++stale >= 3) { rx_state = RXS_SYNC; rx_fidx = 0; stale = 0; }
    } else {
        stale = 0;
    }
}

/* ─── CRSF Task ──────────────────────────────────────────────────── */
void crsf_run(void *arg)
{
    DBG("[CRSF] task OK\r\n");

    uint16_t ch[16];
    for (int i = 0; i < 16; i++) ch[i] = CRSF_CH_MID;

    bool     in_failsafe    = false;
    int32_t  prev_seq       = -1;
    uint32_t seq_repeat_cnt = 0;
    uint32_t log_cnt        = 0;   /* limita o log de canais a ~1 Hz */

    for (;;) {
        uint32_t t0 = HAL_GetTick();

        taskENTER_CRITICAL();
        int16_t  dir_snap      = g_direcao;
        int16_t  thr_snap      = g_throttle;
        uint32_t last_rx_snap  = g_last_rx_tick;
        int32_t  seq_snap      = g_last_seq;
        taskEXIT_CRITICAL();

        /* Detecta seq congelado */
        if (seq_snap == prev_seq) {
            if (seq_repeat_cnt < CRSF_SEQ_REPEAT_MAX)
                seq_repeat_cnt++;
        } else {
            prev_seq       = seq_snap;
            seq_repeat_cnt = 0;
        }

        bool timeout    = (HAL_GetTick() - last_rx_snap) > CRSF_COMM_TIMEOUT_MS;
        bool seq_frozen = (seq_repeat_cnt >= CRSF_SEQ_REPEAT_MAX);
        bool failsafe   = timeout || seq_frozen;

        if (failsafe) {
            dir_snap = 1500;
            thr_snap = 1500;
            if (!in_failsafe) {
                if (timeout)
                    DBG("[CRSF] FAILSAFE — timeout de comunicacao\r\n");
                else
                    DBG("[CRSF] FAILSAFE — seq congelado\r\n");
                in_failsafe = true;
            }
        } else {
            if (in_failsafe) {
                DBG("[CRSF] comunicacao restaurada\r\n");
                in_failsafe = false;
            }
        }

        ch[0] = crsf_from_us_dir(dir_snap);
        ch[1] = crsf_from_us(thr_snap);
        for (int i = 2; i < 16; i++) ch[i] = CRSF_CH_MID;

        /* Log throttled (~1 Hz) — não floodar o laço de 150 Hz */
        if (++log_cnt >= CRSF_RATE_HZ) {
            log_cnt = 0;
            DBG_FMT("[CRSF] CH0=%u CH1=%u%s\r\n",
                    ch[0], ch[1], in_failsafe ? " (FAILSAFE)" : "");
        }

        crsf_send_channels(ch);

        /* Envia ACK — fora da interrupção USB */
        if (g_ack_pending) {
            CDC_Transmit_FS((uint8_t *)g_ack_buf, g_ack_len);
            g_ack_pending = false;
        }

        /* Drena e parseia a telemetria recebida na janela anterior */
        crsf_rx_poll();

        uint32_t elapsed = HAL_GetTick() - t0;
        if (elapsed < CRSF_RATE_MS) osDelay(CRSF_RATE_MS - elapsed);
        else osDelay(1);
    }
}
