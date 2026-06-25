---
title: 🧠 MOC — Cérebro RADIO_MASTER
tags:
  - moc
  - indice
aliases:
  - Home
  - Índice
  - MOC
created: 2026-06-25
---

# 🧠 Cérebro do Projeto RADIO_MASTER

Vault central de contexto, decisões técnicas, testes e conteúdo técnico do firmware **RADIO_MASTER** (STM32F103C8T6 → CRSF).

> [!abstract] O que é o RADIO_MASTER
> Ponte/transmissor que recebe comandos **JSON via USB CDC** de um host (PC), converte para canais RC e transmite **frames CRSF RC_CHANNELS** por USART1 (half-duplex, DMA) para um módulo de rádio Crossfire/ELRS. Inclui debug por USART2 e lógica de **failsafe**.

## 🗺️ Mapa do vault

### Contexto
- [[Visão Geral do Projeto]]
- [[Glossário]]

### Hardware
- [[Pinout STM32F103C8]]
- [[Clock e Alimentação]]
- [[Conexões dos Módulos]]

### Firmware
- [[Arquitetura de Firmware]]
- [[Tasks FreeRTOS]]
- [[Driver CRSF]]
- [[USB CDC (VCP)]]
- [[Debug UART]]

### Protocolo
- [[Protocolo CRSF]]
- [[Protocolo USB JSON]]

### Decisões técnicas
- [[ADR-000 Template]]
- [[ADR-001 Transporte CRSF por USART1 Half-Duplex]]
- [[ADR-002 Host envia JSON via USB CDC]]
- [[ADR-003 Estratégia de Failsafe]]

### Testes
- [[Plano de Testes]]
- [[Log de Testes]]

### Conteúdo técnico
- [[Referências]]
- [[Questões em Aberto]]

## ✅ Estado atual (2026-06-25)

> [!success] Implementado
> - Esqueleto CubeMX + CMake + FreeRTOS (CMSIS-RTOS v2)
> - Clock 72 MHz (HSE 8 MHz ×9), USB 48 MHz
> - USART1 half-duplex 400000 baud + DMA TX → CRSF
> - USART2 115200 → debug bloqueante
> - USB CDC: recebe JSON, devolve ACK
> - Task CRSF a 150 Hz, 2 canais ativos (direção + throttle)
> - Failsafe por timeout e por `seq` congelado

> [!todo] Pendências conhecidas
> Ver [[Questões em Aberto]] — inclui divergência no timeout de failsafe e o busy-wait no TX.

## 🏷️ Tags principais

`#hardware` `#firmware` `#protocolo/crsf` `#protocolo/usb` `#freertos` `#failsafe` `#teste` `#adr`
