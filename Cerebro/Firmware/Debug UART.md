---
title: Debug UART
tags:
  - firmware
  - debug
created: 2026-06-25
---

# Debug UART (`debug.c` / `debug.h`)

Log de texto por **USART2 @ 115200** (PA2 TX). Útil em bancada com Putty/Hercules.

## API
```c
void debug_init(void);
void debug_print(const char *str);
void debug_printf(const char *fmt, ...);
```

## Macros
```c
#define DEBUG_ENABLED 1        // 0 desabilita todo debug (vira no-op)
#define DBG(str)          debug_print(str)
#define DBG_FMT(fmt, ...) debug_printf(fmt, ##__VA_ARGS__)
```

## Características
- `HAL_UART_Transmit` **bloqueante**, timeout 10 ms.
- Buffer interno `_dbg_buf[256]` para `vsnprintf`.
- Quando `DEBUG_ENABLED 0`, as funções viram stubs vazios (custo ~zero).

## Conexão
- **PA2 (USART2_TX)** → RX do adaptador USB-Serial.
- **GND** comum. Ver [[Conexões dos Módulos]].

> [!warning] Custo em tempo real
> Logs bloqueantes dentro da [[Tasks FreeRTOS|CRSF_task]] (ex.: `DBG_FMT("CRSF CH[%d]...")` a cada ciclo) consomem tempo do período de 150 Hz. Recomendado reduzir/condicionar logs no laço quente — ver [[Questões em Aberto]].

## Relacionadas
- [[Pinout STM32F103C8]] · [[Driver CRSF]]
