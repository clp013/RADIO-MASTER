#ifndef CRSF_H
#define CRSF_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ─── Configurações ──────────────────────────────────────────────── */
#define CRSF_RATE_HZ            150
#define CRSF_RATE_MS            (1000 / CRSF_RATE_HZ)
#define CRSF_COMM_TIMEOUT_MS    1000   /* failsafe: sem dados USB por 1000 ms (1 s) → neutro */
#define CRSF_SEQ_REPEAT_MAX     100   /* failsafe: seq congelado por N ciclos CRSF → neutro */

/* Protocolo CRSF */
#define CRSF_SYNC_BYTE          0xEE
#define CRSF_TYPE_RC_CHANNELS   0x16
#define CRSF_RC_FRAME_SIZE      26
#define CRSF_MAX_PACKET_SIZE    64

/* Valores de canal */
#define CRSF_CH_MIN     192
#define CRSF_CH_MID     992
#define CRSF_CH_MAX     1792

/* ─── API pública ────────────────────────────────────────────────── */
void     crsf_init(UART_HandleTypeDef *huart);
void     crsf_send_channels(const uint16_t ch[16]);
void     crsf_run(void *arg);
void     crsf_usb_receive(const char *json);

uint16_t crsf_from_us(int16_t us);
uint16_t crsf_from_us_dir(int16_t us);

#endif /* CRSF