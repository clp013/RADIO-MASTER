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

## ACK + telemetria (placa → host)
Desde 2026-06-26 o ACK carrega a **telemetria** do Ranger Nano (piggyback no mesmo JSON):
```json
{"direcao":1600,"throttle":1700,"seq":42,"ok":1,"tlm":1,
 "lq_u":100,"lq_d":100,"rssi_u":-31,"rssi_d":-40,
 "snr_u":14,"snr_d":13,"pwr":250,"v":5.8,"pct":0}
```
- Enviado pela [[Tasks FreeRTOS|CRSF_task]] após cada comando recebido (request-response). A ISR do USB só sinaliza; a task **monta e envia** (sem corrida com os structs de telemetria).
- Eco dos valores aceitos + `"ok":1`.

| Campo | Origem | Significado |
|-------|--------|-------------|
| `tlm` | g_link.valid | 1 = já recebeu link stats (0 = telemetria ainda não chegou) |
| `lq_u` / `lq_d` | 0x14 | link quality uplink/downlink (%) |
| `rssi_u` / `rssi_d` | 0x14 | RSSI uplink/downlink (dBm, **int8**) |
| `snr_u` / `snr_d` | 0x14 | SNR (dB) |
| `pwr` | 0x14 | potência de TX (mW) |
| `v` | 0x08 | tensão da bateria (V, 1 casa) |
| `pct` | 0x08 | bateria restante (%) |

> [!note] Tamanho
> JSON ≈ 150–175 bytes (buffer 256). Sem floats no firmware: `v` é montado como `%d.%d` (decivolts). Campos zerados com `tlm:0` = telemetria ainda não recebida.

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
