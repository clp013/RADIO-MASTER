---
title: Questões em Aberto
tags:
  - backlog
  - bug
created: 2026-06-25
---

# Questões em Aberto

Itens detectados na leitura do código (2026-06-25). Cada um deve virar uma correção ou um ADR.

> [!success] Q1 — Timeout de failsafe ✅ RESOLVIDO (2026-06-25)
> Era `CRSF_COMM_TIMEOUT_MS = 50000` (50 s) vs comentário "500 ms". **Definido em 1000 ms (1 s)** e comentário alinhado em `Core/Inc/crsf.h` (linha 11). → [[ADR-003 Estratégia de Failsafe]].

> [!warning] Q2 — Busy-wait em task de alta prioridade
> `crsf_send_channels` faz `while(!tx_done && (HAL_GetTick()-t)<2)` após o DMA. Espera ocupada de até ~2 ms numa task `osPriorityHigh` pode afamar a defaultTask/USB. **Ação:** avaliar semáforo/notify a partir do `TxCpltCallback` em vez de spin. → [[Tasks FreeRTOS]]

> [!warning] Q3 — Logs no laço quente
> `DBG_FMT("CRSF CH[%d]...")` a cada ciclo (150 Hz) por USART2 bloqueante (~) consome tempo do período. **Ação:** remover/condicionar no caminho normal. → [[Debug UART]]

> [!question] Q4 — Endereço/sync CRSF
> `CRSF_SYNC_BYTE = 0xEE` (transmitter). Confirmar se o módulo alvo espera `0xEE` ou `0xC8` (FC). → [[Protocolo CRSF]]

> [!question] Q5 — Telemetria (RX) não usada
> USART1 é half-duplex mas só TX é consumido. Definir se haverá leitura de telemetria do módulo. → [[ADR-001 Transporte CRSF por USART1 Half-Duplex]]

> [!note] Q6 — `debug_init()` vazio
> `debug_init()` não configura nada (USART2 é init via HAL em `main`). Verificar se é intencional manter a função como placeholder.

> [!note] Q7 — Granularidade do tick vs período
> `CRSF_RATE_MS = 1000/150 = 6` (trunca 6,67). Período real ~6 ms → ~166 Hz, não 150 Hz exato. Avaliar se importa para o link.

## Relacionadas
- [[🧠 MOC - Cérebro RADIO_MASTER]]