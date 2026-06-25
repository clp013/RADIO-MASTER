---
title: ADR-001 Transporte CRSF por USART1 Half-Duplex
tags:
  - adr
  - protocolo/crsf
status: aceita
created: 2026-06-25
---

# ADR-001 — Transporte CRSF por USART1 Half-Duplex

## Status
`aceita` (refletido no código atual)

## Contexto
O módulo Crossfire/ELRS usa **uma única linha de sinal** bidirecional para CRSF. O STM32F103 não tem UART dedicada com pino de telemetria separado para esse uso; o esquema mais simples é half-duplex em PA9.

## Decisão
Usar **USART1 em modo half-duplex** (`HAL_HalfDuplex_Init`) no pino **PA9**, a **400000 baud**, com **DMA TX** (`DMA1_Channel4`) para envio dos frames RC_CHANNELS. Alterna-se `EnableTransmitter`/`EnableReceiver` em torno de cada transmissão.

## Alternativas consideradas
- **Full-duplex (PA9+PA10):** exigiria conversão externa; o módulo é single-wire. Descartado.
- **TX por polling (sem DMA):** bloqueia a CPU por ~0,65 ms por frame; DMA libera a CPU. Mantido DMA.
- **Software serial / timer bit-bang:** complexidade desnecessária.

## Consequências
- Positivas: hardware mínimo (1 fio + GND), TX eficiente por DMA.
- Negativas / dívida técnica:
  - Há um **busy-wait** (`while(!tx_done ... <2ms)`) após o DMA para reabilitar o receptor, dentro de uma task de alta prioridade. Ver [[Tasks FreeRTOS]] e [[Questões em Aberto]].
  - RX (telemetria) ainda **não** é consumido.

## Relacionadas
- [[Driver CRSF]] · [[Protocolo CRSF]] · [[Pinout STM32F103C8]]
