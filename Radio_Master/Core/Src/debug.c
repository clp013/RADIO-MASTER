/**
 * debug.c — Implementação do debug via USART2
 */

#include "debug.h"
#include <stdarg.h>

#if DEBUG_ENABLED

static char _dbg_buf[256];

void debug_init(void)
{
    
}

void debug_print(const char *str)
{
    if (!str) return;
    uint16_t len = (uint16_t)strlen(str);
    if (len == 0) return;
    /* Bloqueante — simples e confiável para debug */
    HAL_UART_Transmit(&huart2, (uint8_t *)str, len, 10);
}

void debug_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(_dbg_buf, sizeof(_dbg_buf), fmt, args);
    va_end(args);
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t *)_dbg_buf, (uint16_t)len, 10);
    }
}

#else
/* Debug desabilitado — funções vazias */
void debug_init(void)   {}
void debug_print(const char *str) { (void)str; }
void debug_printf(const char *fmt, ...) { (void)fmt; }
#endif
