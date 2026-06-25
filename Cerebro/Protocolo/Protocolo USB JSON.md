---
title: Protocolo USB JSON
tags:
  - protocolo/usb
created: 2026-06-25
---

# Protocolo USB JSON (Host → STM32)

Comandos enviados pelo host pela [[USB CDC (VCP)|porta serial virtual]], uma linha JSON por comando, **terminada em `\n`**.

## Comando (host → placa)
```json
{"direcao":1500,"throttle":1500,"seq":42}
```
| Campo | Tipo | Faixa | Default | Significado |
|-------|------|-------|---------|-------------|
| `direcao` | int | 1000–2000 | 1500 | Pulso de direção (µs) → CH0 |
| `throttle` | int | 1000–2000 | 1500 | Pulso de throttle (µs) → CH1 |
| `seq` | int | qualquer | 0 | Contador de sequência (anti-travamento) |

- Valores fora da faixa são **saturados** no firmware (`crsf_usb_receive`).
- Parse é manual (`json_get_int`) — tolera espaços após `:`, ignora chaves desconhecidas.

## ACK (placa → host)
```json
{"direcao":1500,"throttle":1500,"seq":42,"ok":1}
```
- Enviado pela [[Tasks FreeRTOS|CRSF_task]] após processar o comando.
- Eco dos valores aceitos + `"ok":1`.

## Regras importantes
> [!important] Terminador `\n` obrigatório
> O firmware só processa o JSON quando recebe `'\n'`. Cada comando deve terminar com nova-linha.

> [!tip] Use `seq` crescente
> Incrementar `seq` a cada envio evita o failsafe por "seq congelado" ([[ADR-003 Estratégia de Failsafe]]). Reenviar o mesmo `seq` por ≥100 ciclos CRSF é interpretado como host travado.

## Exemplo de teste (terminal)
```
{"direcao":1600,"throttle":1500,"seq":1}
{"direcao":1600,"throttle":1700,"seq":2}
```

## Relacionadas
- [[USB CDC (VCP)]] · [[Driver CRSF]] · [[Plano de Testes]]
