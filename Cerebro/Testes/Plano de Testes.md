---
title: Plano de Testes
tags:
  - teste
created: 2026-06-25
---

# Plano de Testes

Roteiro de validação de bancada do [[Visão Geral do Projeto|RADIO_MASTER]]. Resultados vão em [[Log de Testes]].

## 1. Bring-up básico
- [ ] Placa enumera como porta serial (CDC) no PC.
- [ ] LED PC13 piscando ~5 Hz (heartbeat da [[Tasks FreeRTOS|defaultTask]]).
- [ ] Debug USART2 (115200) imprime `[CRSF] Init OK — 150Hz` e `[CRSF] task OK`.

## 2. Caminho USB → parse
- [ ] Enviar `{"direcao":1600,"throttle":1500,"seq":1}\n` → receber ACK com `"ok":1`.
- [ ] Valores fora de faixa (ex.: 3000) são saturados a 2000.
- [ ] JSON sem `\n` **não** dispara processamento (confirma framing).

## 3. Saída CRSF (osciloscópio / analisador lógico em PA9)
- [ ] Frame de 26 bytes, inicia em `0xEE`, `len=0x18`, `type=0x16`.
- [ ] Taxa ≈ 150 Hz (período ~6,67 ms).
- [ ] CRC8 confere (poly 0xD5).
- [ ] CH0/CH1 variam conforme `direcao`/`throttle`; CH2–15 em 992.

## 4. Failsafe ([[ADR-003 Estratégia de Failsafe]])
- [ ] Parar envios USB → canais vão a 992 (neutro) após o timeout. **Medir o tempo real** (verificar divergência 500 ms vs 50 s).
- [ ] Manter `seq` fixo enviando → failsafe por "seq congelado" após ~100 ciclos.
- [ ] Retomar envios com `seq` crescente → log `comunicacao restaurada`, canais voltam a seguir o host.

## 5. Integração com módulo RF
- [ ] Módulo reconhece os frames (LED/telemetria do módulo).
- [ ] Receptor/veículo responde a direção e throttle.
- [ ] Confirmar endereço esperado (`0xEE` vs `0xC8`).

## 6. Robustez / timing
- [ ] Logs intensos na CRSF_task não quebram o período de 150 Hz.
- [ ] Sessão prolongada (>30 min) sem travar nem perder enumeração USB.

## Equipamento
Analisador lógico/osciloscópio, adaptador USB-Serial, módulo Crossfire/ELRS, fonte do módulo.

## Relacionadas
- [[Log de Testes]] · [[Protocolo CRSF]] · [[Protocolo USB JSON]]
