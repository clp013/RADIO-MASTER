---
title: Referências
tags:
  - referencia
created: 2026-06-25
---

# Referências

## Protocolo CRSF
- TBS CRSF (Crossfire) — especificação do protocolo serial.
- ExpressLRS (ELRS) — documentação do CRSF e taxas de pacote: https://www.expresslrs.org
- `RC_CHANNELS_PACKED` (type `0x16`): 16 canais × 11 bits.

## STM32 / HAL
- Reference Manual **RM0008** (STM32F101/102/103/105/107).
- Datasheet STM32F103C8T6.
- STM32F1 HAL — `HAL_HalfDuplex_Init`, `HAL_UART_Transmit_DMA`, `HAL_HalfDuplex_EnableTransmitter/Receiver`.

## USB CDC
- STM32 USB Device Library (Class CDC).
- AN para USB FS no STM32F1 (clock 48 MHz a partir do PLL ÷1.5).

## FreeRTOS / CMSIS-RTOS
- CMSIS-RTOS v2 API (`osThreadNew`, `osDelay`, `osKernelStart`).
- FreeRTOS `taskENTER_CRITICAL` / `configASSERT`.

## Ferramentas do projeto
- STM32CubeMX (geração do `.ioc`).
- CMake + Ninja + `arm-none-eabi-gcc` (ver `CMakePresets.json`).

> [!note] Coloque PDFs/capturas em `Conteudo Tecnico/anexos/` e embuta com `![[arquivo.pdf]]`.

## Relacionadas
- [[Protocolo CRSF]] · [[Clock e Alimentação]] · [[Arquitetura de Firmware]]
