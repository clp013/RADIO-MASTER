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

> [!success] Q5 — Telemetria (RX) — RECEPÇÃO FUNCIONANDO (2026-06-25)
> RX implementado e **validado em bancada** ([[ADR-004 Recepção de Telemetria (modo dump)]]). Frames reais confirmados: `0x3A` (timing), `0x14` (link stats), `0x08` (battery) — todos CRC OK. Falta o **parse estruturado** dos payloads e expô-los (próxima etapa). Lição: não diagnosticar RX com o analisador na linha (interfere). → [[Telemetria CRSF (RX)]]
