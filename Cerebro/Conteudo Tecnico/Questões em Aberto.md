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

> [!success] Q2 — Busy-wait ✅ RESOLVIDO (2026-06-25)
> O `while(!tx_done ...)` foi substituído por **notificação de tarefa**: `TxCpltCallback` chama `vTaskNotifyGiveFromISR` e `crsf_send_channels` bloqueia em `ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2))`. A task não gira mais ocupada; o scheduler libera a CPU durante o DMA. Verificado que a IRQ do DMA (prio 5) está dentro de `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5). → [[Tasks FreeRTOS]]

> [!success] Q3 — Logs no laço quente ✅ RESOLVIDO (2026-06-25)
> O `DBG_FMT` por ciclo virou log **throttled a ~1 Hz** (`if (++log_cnt >= CRSF_RATE_HZ)`), imprimindo CH0/CH1 e estado de failsafe. → [[Debug UART]]

> [!success] Q4 — Endereço/sync CRSF ✅ CONFIRMADO (2026-06-25)
> `CRSF_SYNC_BYTE = 0xEE` está **correto** para o módulo alvo (endereço transmitter). Nenhuma alteração necessária. → [[Protocolo CRSF]]

> [!success] Q5 — Telemetria (RX) — CONCLUÍDA (2026-06-26)
> Pipeline completo e validado: RX ([[ADR-004 Recepção de Telemetria (modo dump)]]) → parse estruturado de `0x14`/`0x08`/`0x3A` (structs `g_link`/`g_batt`/`g_timing`) → **exportada ao PC** no ACK JSON da USB ([[Protocolo USB JSON]]). Lição: não diagnosticar RX com o analisador na linha (interfere). Resta opcional: **sincronismo de fase** usando o `offset` do timing → ADR-005.

> [!note] Q6 — `debug_init()` vazio
> `debug_init()` não configura nada (USART2 é init via HAL em `main`). Verificar se é intencional manter a função como placeholder.

> [!success] Q7 — Taxa real de 150 Hz ✅ RESOLVIDO (2026-06-28)
> Era `CRSF_RATE_MS = 1000/150 = 6` (~166 Hz). Agora o período vem do `interval` do módulo (0x3A) com acumulador fracionário → média 6.666 ms (150 Hz). Isso casou a taxa e **travou a fase** (offset preso em ~−300 µs em vez de varrer ±6 ms). Ver [[ADR-005 Sincronismo de Fase (casamento de taxa)]].

## Relacionadas
- [[🧠 MOC - Cérebro RADIO_MASTER]] · [[Plano de Testes]]
