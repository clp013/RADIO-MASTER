---
title: Pinout STM32F103C8
tags:
  - hardware
  - pinout
created: 2026-06-25
---

# Pinout STM32F103C8T6

Mapeamento de pinos confirmado a partir de `main.c`, `main.h` e `stm32f1xx_hal_msp.c`.

| Pino | Função | Periférico | Detalhe |
|------|--------|------------|---------|
| **PA9**  | USART1_TX | [[Driver CRSF\|CRSF]] | Half-duplex, 400000 baud, DMA TX (saída CRSF) |
| **PA2**  | USART2_TX | [[Debug UART\|Debug]] | 115200 baud, TX bloqueante |
| **PA3**  | USART2_RX | Debug | Reservado (RX não usado no firmware) |
| **PA11** | USB_DM | USB CDC | D− (interno ao MCU) |
| **PA12** | USB_DP | USB CDC | D+ (interno ao MCU) |
| **PC13** | LED | GPIO saída | Heartbeat (`LED_Pin`, ativo conforme placa) |
| **PD0/PD1** | OSC_IN/OSC_OUT | HSE | Cristal 8 MHz |

> [!note] Definições em código
> `LED_Pin = GPIO_PIN_13`, `LED_GPIO_Port = GPIOC` (em `Core/Inc/main.h`).

## USART1 (CRSF) — half-duplex
- Inicializado com `HAL_HalfDuplex_Init(&huart1)` — apenas **PA9** é usado (TX/RX no mesmo fio).
- DMA: `DMA1_Channel4` (TX). IRQ `DMA1_Channel4_IRQn` e `DMA1_Channel5_IRQn` habilitadas com prioridade 5.
- Ver fluxo de transmissão em [[Driver CRSF]].

## USART2 (Debug)
- TX em **PA2** → ligar ao **RX** de um adaptador USB-Serial (Putty/Hercules).
- Sem DMA; `HAL_UART_Transmit` bloqueante com timeout 10 ms.

## USB
- USB Device Full-Speed nativo (PA11/PA12). Requer clock de 48 MHz — ver [[Clock e Alimentação]].

## Relacionadas
- [[Conexões dos Módulos]]
- [[Clock e Alimentação]]
- [[Arquitetura de Firmware]]
