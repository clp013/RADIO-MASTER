---
title: ADR-004 Recepção de Telemetria (modo dump)
tags:
  - adr
  - protocolo/crsf
  - rx
status: aceita
created: 2026-06-25
---

# ADR-004 — Recepção de Telemetria CRSF (modo dump)

## Status
`aceita` — **validado em bancada (2026-06-25)**: recebe `0x3A`, `0x14` e `0x08`, todos com CRC OK.

> [!warning] Lição de bancada — analisador lógico interfere
> A ponta do analisador na linha single-wire (capacitância/stub) degrada as bordas a 400 kbaud **quando o STM32 também recebe**, derrubando o link. Diagnosticar RX **sem** o analisador conectado; usar o próprio dump como instrumento.

## Contexto
Precisamos receber a [[Telemetria CRSF (RX)|telemetria do Ranger Nano]] no fio único de USART1. O RX-DMA circular já está configurado pelo CubeMX, mas o TX usa `HAL_UART_Transmit_DMA` + `HAL_HalfDuplex_Enable*`, e misturar o estado da HAL (gState/RxState) entre TX-DMA e RX-DMA no half-duplex é frágil.

## Decisão
Implementar o RX por **interrupção RXNE em nível de registrador + ring buffer**, isolado da máquina de estados da HAL:

- `crsf_uart_irq_handler()` (em `crsf.c`) lê `USART1->DR` (limpa RXNE e flags de erro) e empilha no `rx_ring[256]`. É chamada de `USART1_IRQHandler` (em `stm32f1xx_it.c`, `USER CODE BEGIN USART1_IRQn 0`) **antes** de `HAL_UART_IRQHandler`, que vira no-op.
- `__HAL_UART_ENABLE_IT(huart1, UART_IT_RXNE)` em `crsf_init` (NVIC `USART1_IRQn` já habilitado pelo MSP, prio 5).
- `crsf_rx_poll()` roda no laço de `crsf_run` (fora da ISR): drena o ring e alimenta um **parser de máquina de estados** (`SYNC → LEN → DATA`) que valida `len` (2–62) e **CRC8** (reuso de `crsf_crc8`), com reset anti-travamento para frames parciais.
- Primeiro entregável = **dump**: loga `"[RX] type=0x.. len=.. crc=OK/ERR"` na USART2 (flags `CRSF_RX_DEBUG`/`CRSF_RX_HEXDUMP`).

## Alternativas consideradas
- **RX-DMA circular + IDLE (HAL):** mais "padrão", mas entrelaça estados da HAL com o TX-DMA no half-duplex → risco de `HAL_BUSY`/corrida. Descartado para o dump (pode ser revisitado depois).
- **HAL_UART_Receive_IT 1 byte:** usa a máquina de estados da HAL (RxState BUSY_RX) que conflita com o `EnableTransmitter/Receiver`. Descartado.

## Consequências
- Positivas: independente da HAL no caminho de RX; robusto à troca TX↔RX; CPU desprezível (telemetria ~4,8 Hz). Valida tudo o que o analisador mostrou, no próprio MCU.
- Negativas / notas:
  - `USART1_IRQHandler` agora tem código em `USER CODE` — se reabrir o CubeMX, manter a chamada `crsf_uart_irq_handler()`.
  - Log por USART2 é bloqueante (~2 ms); cai em ~10 frames/s → jitter ocasional no laço de 150 Hz, aceitável em debug. Manter `CRSF_RX_HEXDUMP=0` no uso normal.
  - Parser aceita qualquer sync (valida por len+CRC); mis-sync por ruído se auto-corrige.

## Próximo
Parse estruturado de `0x14` (RSSI **int8**, LQ, SNR, potência) e `0x3A` (timing); expor via [[Protocolo USB JSON]]. Vira ADR-005 (sincronismo de fase) se formos usar o `offset`.

## Relacionadas
- [[Recepção CRSF Half-Duplex]] · [[Telemetria CRSF (RX)]] · [[Driver CRSF]]
