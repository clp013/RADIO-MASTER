/**
 * debug.h — Debug via USART2 @ 115200 baud
 *
 * Conexão:
 *   PA2 (USART2_TX) → RX do adaptador USB-Serial → Hercules/Putty
 *   GND             → GND
 *
 * Uso:
 *   DBG("Iniciando...\r\n");
 *   DBG_FMT("CH1=%d CH2=%d\r\n", ch[0], ch[1]);
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ─── Habilita/desabilita debug ──────────────────────────────────── */
#define DEBUG_ENABLED   1   // 0 = desabilita todo debug

/* ─── Handle da USART2 — declarado no main.c ─────────────────────── */
extern UART_HandleTypeDef huart2;

/* ─── Funções ────────────────────────────────────────────────────── */
void debug_init(void);
void debug_print(const char *str);
void debug_printf(const char *fmt, ...);

/* ─── Macros convenientes ────────────────────────────────────────── */
#if DEBUG_ENABLED
    #define DBG(str)           debug_print(str)
    #define DBG_FMT(fmt, ...)  debug_printf(fmt, ##__VA_ARGS__)
#else
    #define DBG(str)           ((void)0)
    #define DBG_FMT(fmt, ...)  ((void)0)
#endif

#endif /* DEBUG_H */
