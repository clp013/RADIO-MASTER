---
title: ADR-006 Robustez de RX (reforço de RXNEIE + watchdog)
tags:
  - adr
  - firmware
  - rx
status: aceita
created: 2026-06-28
---

# ADR-006 — Robustez de RX (reforço de RXNEIE + watchdog)

## Status
`aceita` — validado em bancada (2026-06-28), `rearm=0` (reforço de RXNEIE bastou).

## Contexto
A recepção de telemetria **congelava de forma intermitente** após minutos de uso: na USART2 só sobrava `[CRSF]` (laço vivo), os `[LINK]/[BATT]` paravam (recepção morta), e **só um reset recuperava**.

Diagnóstico: o `crsf_send_channels` re-habilita `RE` a cada ciclo (`HAL_HalfDuplex_EnableReceiver`), mas **não** o `RXNEIE`. Numa **corrida de overrun (ORE)**, o `HAL_UART_IRQHandler` (chamado após a nossa ISR em `USART1_IRQHandler`) pode **limpar o `RXNEIE`**. Sem `RXNEIE`, a interrupção de RX nunca mais dispara → RX morto, e o `EnableReceiver` por ciclo (só `RE`) não recupera.

## Decisão
Duas camadas em `crsf.c`:

1. **Reforço de `RXNEIE` a cada ciclo** — em `crsf_send_channels`, logo após `HAL_HalfDuplex_EnableReceiver`, chamar `__HAL_UART_ENABLE_IT(_huart, UART_IT_RXNE)`. Idempotente e barato; garante que a interrupção de RX nunca fique desligada. **É a correção de raiz.**
2. **Watchdog de RX** (rede de segurança) — em `crsf_run`, se `rx_byte_cnt` não muda por ~1 s (`>= CRSF_RATE_HZ` ciclos sem byte), re-arma: lê `SR`+`DR` (limpa ORE), `EnableReceiver`, `RXNEIE`; incrementa `rearm` (exposto no log `[CRSF]`). Nunca dispara em operação normal (telemetria chega a cada ~200 ms).

## Alternativas consideradas
- **Não chamar `HAL_UART_IRQHandler`** em `USART1_IRQn` (eliminar a corrida): inviável — a HAL trata o **TC** (fim de TX por DMA) que dispara a nossa notificação. Descartado.
- **Só watchdog (sem reforço por ciclo):** recupera, mas deixa ~1 s de RX morto a cada trava. O reforço por ciclo evita a trava por completo.

## Consequências
- Positivas: RX **auto-recupera**, sem reset; causa raiz eliminada. Bancada: `rearm=0` (a trava nem ocorre mais). Custo desprezível (uma escrita de registrador por ciclo).
- Observação: o `rearm` no `[CRSF]` é um bom indicador de saúde — se um dia subir, é sinal de trava residual sendo recuperada pelo watchdog.

## Relacionadas
- [[Recepção CRSF Half-Duplex]] · [[Driver CRSF]] · [[Log de Testes]] (2026-06-28)
