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

## Saídas placa → host (dois payloads distintos)

> [!warning] Revisão 2026-06-27 — telemetria separada do ACK
> A 1ª tentativa embutia a telemetria no ACK **a cada comando**; isso (JSON grande no laço quente) atrapalhava a cadência de 150 Hz e derrubava o enlace de RF (LQ→0). Revertido. Agora há **dois payloads independentes**.

### 1) ACK do comando (request-response, leve)
Montado na ISR do USB, enviado pela task após cada comando:
```json
{"direcao":1600,"throttle":1700,"seq":42,"ok":1}
```
Apenas eco do comando + `"ok":1`. Sem telemetria.

### 2) Telemetria (periódico, ~1 Hz, independente de comando/failsafe)
Montado e enviado pela [[Tasks FreeRTOS|CRSF_task]] uma vez por segundo, **desacoplado** do caminho por-comando:
```json
{"tlm":1,"lq_u":100,"lq_d":100,"rssi_u":-42,"rssi_d":-49,"snr_u":14,"snr_d":13,"pwr":250,"v":5.8,"pct":0}
```

| Campo | Origem | Significado |
|-------|--------|-------------|
| `tlm` | g_link.valid | 1 = já recebeu link stats (0 = telemetria ainda não chegou) |
| `lq_u` / `lq_d` | 0x14 | link quality uplink/downlink (%) |
| `rssi_u` / `rssi_d` | 0x14 | RSSI uplink/downlink (dBm, **int8**) |
| `snr_u` / `snr_d` | 0x14 | SNR (dB) |
| `pwr` | 0x14 | potência de TX (mW) |
| `v` | 0x08 | tensão da bateria (V, 1 casa) |
| `pct` | 0x08 | bateria restante (%) |

- O host distingue os dois pela presença de `lq_u` (telemetria) vs `direcao` (ACK).
- JSON da telemetria ≈ 106 B (buffer 160). Sem floats: `v` montado como `%d.%d` (decivolts).
- **Lição:** trabalho pesado por-comando no laço de 150 Hz derruba o enlace; telemetria a baixa taxa e desacoplada resolve. Ver [[Log de Testes]] 2026-06-27.

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
