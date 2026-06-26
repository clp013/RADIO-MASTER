---
title: ADR-005 Sincronismo de Fase (casamento de taxa)
tags:
  - adr
  - firmware
  - protocolo/crsf
  - timing
status: aceita
created: 2026-06-28
---

# ADR-005 — Sincronismo de Fase (casamento de taxa)

## Status
`aceita` — validado em bancada (2026-06-28). Resolve também o [[Questões em Aberto|Q7]] (taxa real de 150 Hz).

## Contexto
O firmware transmitia os canais a **~166 Hz** (`CRSF_RATE_MS = 1000/150 = 6`, truncado), enquanto o módulo opera a **150 Hz** (`interval` = 6666 µs, informado no frame `0x3A`). Mandando mais rápido que a janela de RF do módulo, a **fase escorregava continuamente** — o `offset` do `0x3A` varria ±6 ms o tempo todo, somando latência variável.

## Decisão
**Casar a taxa do TX ao `interval` que o módulo informa**, em vez de uma correção ativa por offset.

- No laço de `crsf_run`, o período passa a vir de `g_timing.interval_100ns/10` (≈6666 µs), com **acumulador fracionário** (`frac_us`) que alterna períodos de 6/7 ms para a **média fechar 6.666 ms** (150 Hz). Fallback `1000000/CRSF_RATE_HZ` enquanto a telemetria não chegou.
- O `crsf_send_channels` foi movido para **cedo no laço** (TX determinístico); o heartbeat de debug ficou **depois** do TX.

## Por que NÃO fizemos correção ativa (Passo 2)
Com o casamento de taxa, a fase **já trava**: o `offset` fica preso em **~−300 µs** (4,5% do período). Medindo a distribuição (avg/min/max), descobrimos que os "picos" de +4000 µs eram **artefato do próprio debug**: um log bloqueante de ~5 ms atrasava um pacote ~2,3 ms, o que equivale a **+4,3 ms adiantado para a janela seguinte** (wrap-around). O `min` (~−300 µs, pacotes não-perturbados) é a fase real. Logo:
- A correção ativa traria ganho marginal (zerar −300 µs).
- E ficaria "perseguindo" os picos artificiais do log → risco de piorar.

Conclusão: **casar a taxa = sincronismo de fase suficiente.** Debug verboso (`CRSF_RX_DEBUG`) desligado em produção para o laço ficar enxuto.

## Consequências
- Positivas: fase travada perto de zero (de ±6 ms varrendo → ~−300 µs preso); latência baixa e consistente; resolve Q7. Sem hardware novo (resolução de tick / FreeRTOS).
- Limite: precisão ~±1 ms (tick do FreeRTOS). Para µs-perfeito precisaria de DWT/timer de hardware — não justificado aqui.
- Nota: logar na USART2 a 1 Hz ainda causa um pequeno distúrbio uma vez por segundo; em produção mantém-se mínimo.

## Relacionadas
- [[Telemetria CRSF (RX)]] · [[Driver CRSF]] · [[Tasks FreeRTOS]] · [[Log de Testes]] (2026-06-28)
