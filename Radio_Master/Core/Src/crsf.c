/**
 * crsf.c — CRSF Half-Duplex STM32F103C8T6
 */

#include "crsf.h"
#include "debug.h"
#include "cmsis_os.h"
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

/* Flag TX completo */
static volatile bool tx_done = false;

/* Failsafe — tick e sequência da última recepção USB válida */
static volatile uint32_t g_last_rx_tick = 0;
static volatile int32_t  g_last_seq     = -1;

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

/* ─── Callback TX CRSF ───────────────────────────────────────────── */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) tx_done = true;
}

/* ─── Inicialização ──────────────────────────────────────────────── */
void crsf_init(UART_HandleTypeDef *huart)
{
    _huart = huart;
    DBG_FMT("[CRSF] Init OK — %dHz\r\n", CRSF_RATE_HZ);
    HAL_HalfDuplex_EnableReceiver(_huart);
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
    tx_done = false;
    HAL_HalfDuplex_EnableTransmitter(_huart);
    HAL_UART_Transmit_DMA(_huart, buf, CRSF_RC_FRAME_SIZE);
    uint32_t t = HAL_GetTick();
    while (!tx_done && (HAL_GetTick() - t) < 2);
    HAL_HalfDuplex_EnableReceiver(_huart);
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
        for (int i = 0; i < 2; i++){
        DBG_FMT("CRSF CH[%d]: %d\n\r", i, ch[i]);}
        crsf_send_channels(ch);

        /* Envia ACK — fora da interrupção USB */
        if (g_ack_pending) {
            CDC_Transmit_FS((uint8_t *)g_ack_buf, g_ack_len);
            g_ack_pending = false;
        }

        uint32_t elapsed = HAL_GetTick() - t0;
        if (elapsed < CRSF_RATE_MS) osDelay(CRSF_RATE_MS - elapsed);
        else osDelay(1);
    }
}
