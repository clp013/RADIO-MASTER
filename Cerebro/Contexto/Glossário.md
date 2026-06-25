---
title: Glossário
tags:
  - contexto
  - glossario
created: 2026-06-25
---

# Glossário

> [!info] Termos do projeto e do domínio RC/RF.

**CRSF** — *Crossfire Serial Protocol*. Protocolo serial da TBS usado por Crossfire e ELRS para enviar canais RC e telemetria. Ver [[Protocolo CRSF]].

**ELRS** — *ExpressLRS*. Sistema de link de rádio open-source que consome frames CRSF de um handset/master.

**RC_CHANNELS / RC_CHANNELS_PACKED** — Frame CRSF tipo `0x16` que empacota 16 canais em 11 bits cada (176 bits = 22 bytes).

**Half-duplex** — Modo em que TX e RX compartilham o mesmo fio (USART1 / PA9). Alterna-se entre transmissor e receptor por software. Ver [[Driver CRSF]].

**VCP / CDC** — *Virtual COM Port* / *Communications Device Class*. A placa aparece como porta serial no PC via USB. Ver [[USB CDC (VCP)]].

**Failsafe** — Estado seguro assumido quando a comunicação falha: todos os canais vão para o neutro (1500 µs → `CRSF_CH_MID` = 992). Ver [[ADR-003 Estratégia de Failsafe]].

**seq** — Contador de sequência enviado pelo host em cada JSON. Se repetir por muitos ciclos, indica host travado → failsafe.

**µs (microssegundos de pulso)** — Convenção RC clássica: 1000 µs = mínimo, 1500 µs = centro, 2000 µs = máximo. Convertido para unidades CRSF (192–992–1792).

**HSE** — *High-Speed External*. Cristal externo de 8 MHz usado como fonte do PLL. Ver [[Clock e Alimentação]].

**HAL / MSP** — *Hardware Abstraction Layer* da ST e *MSP* (MCU Support Package), onde clocks/GPIO/IRQ de cada periférico são configurados (`stm32f1xx_hal_msp.c`).

**ADR** — *Architecture Decision Record*. Registro curto de uma decisão técnica e seu porquê. Ver [[ADR-000 Template]].

**MOC** — *Map of Content*. Nota índice que organiza o vault. Ver [[🧠 MOC - Cérebro RADIO_MASTER]].
