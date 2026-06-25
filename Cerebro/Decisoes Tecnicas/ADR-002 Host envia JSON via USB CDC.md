---
title: ADR-002 Host envia JSON via USB CDC
tags:
  - adr
  - protocolo/usb
status: aceita
created: 2026-06-25
---

# ADR-002 — Host envia comandos JSON via USB CDC

## Status
`aceita`

## Contexto
Precisa-se de um canal simples para o PC enviar setpoints (direção, throttle) à placa e receber confirmação, sem ferramentas especiais.

## Decisão
Expor a placa como **USB CDC (VCP)** e trafegar **JSON por linha** terminado em `\n`. O firmware faz parse leve em `crsf_usb_receive` e responde com um **ACK JSON**. Ver [[Protocolo USB JSON]].

## Alternativas consideradas
- **Binário compacto:** mais eficiente, porém menos depurável manualmente. JSON facilita testes em terminal.
- **Comandos pela USART2:** ocuparia o canal de debug; USB já está disponível e não compete com o debug.

## Consequências
- Positivas: fácil de testar (Putty/scripts), legível, extensível (basta adicionar chaves).
- Negativas: parse manual de string em ISR; custo de `snprintf`/`strstr`. Manter JSONs curtos.
  - O parse roda em **contexto de interrupção USB** — manter leve e não bloquear.

## Relacionadas
- [[USB CDC (VCP)]] · [[Protocolo USB JSON]] · [[Driver CRSF]]
