---
title: Log de Testes
tags:
  - teste
  - log
created: 2026-06-25
---

# Log de Testes

> Registro datado de bancadas. Use o template abaixo para cada sessão. Mais recente no topo.

## Template de entrada
```md
### AAAA-MM-DD — <título da sessão>
- **Firmware/commit:** 
- **Hardware:** placa, módulo, alimentação
- **Objetivo:** 
- **Procedimento:** 
- **Resultado:** ✅ / ⚠️ / ❌
- **Medições:** (taxa, tempos, capturas)
- **Observações / próximos passos:** 
```

---

### 2026-06-27 — "Congelamento" diagnosticado: era o caminho de envio ao PC
- **Sintoma:** telemetria no PC "congelava" após rodar um tempo (valores parados); só voltava com reset.
- **Investigação:** instrumentei `[ACK->PC]` + `[RXSTAT]` (isr/byte/ore/ok/err + `CR1`/`SR`). No congelamento: `[RXSTAT] isr=3773 byte=3773 ok=217 cr1=0x2024 sr=0x00D0` → **RX 100% saudável** (RE+RXNEIE ligados, sem ORE preso, frames sendo decodificados). O que estava "parado" eram os valores porque **`LQ=0%`** — o enlace de RF tinha caído. Reset do STM32 **não** trazia o LQ de volta (receptor pareado e ligado).
- **Passo de isolamento:** revertido `crsf.c` p/ `a9d3b6b` (estado **pré-envio ao PC**: telemetria decodificada e logada na USART2, ACK simples na ISR, sem telemetria no ACK e sem debug).
- **Resultado:** ✅ **com a base pré-PC, LQ=100% estável e telemetria viva** (BATT acompanhou 6.6→5.9→6.5→5.8; RSSI variando). 
- **Conclusão:** o congelamento foi **introduzido pelo caminho de envio ao PC** — montar JSON grande de telemetria (e, nos builds de debug, log bloqueante `[ACK->PC]`) **por comando** dentro do laço de 150 Hz atropela a cadência dos canais → módulo perde o enlace (LQ→0). Não é bug de RX.
- **Rigor pendente:** confirmar LQ=100% com a base simples **enquanto o PC envia comandos** (este log estava em FAILSAFE).
- **Re-abordagem do envio ao PC:** enviar telemetria **periodicamente a baixa taxa**, desacoplada do caminho por-comando/canais, sem logs bloqueantes no laço quente.
- **Implementado (2026-06-27):** payload de telemetria **separado, a ~1 Hz** (`g_tlm_buf`), enviado após `crsf_rx_poll()`, independente de comando/failsafe; ACK por-comando mantido leve. JSON ≈ 106 B. Ver [[Protocolo USB JSON]].
- **Resultado:** ✅ **estável** — link 100% e telemetria viva no PC sob fluxo de comandos (sem o congelamento de antes). *Recomendado soak-test mais longo antes de declarar resolvido em definitivo, já que o congelamento anterior só aparecia após minutos.*

### 2026-06-26 — Envio de telemetria ao PC (ACK JSON) — sem bancada
- **Arquivos:** `Core/Src/crsf.c`.
- **O quê:** o ACK da USB agora carrega a telemetria (link stats + bateria). A montagem do ACK saiu da ISR do USB para a **CRSF_task** (a ISR só sinaliza `g_ack_pending`); a task lê `g_link`/`g_batt`, monta o JSON e envia por `CDC_Transmit_FS`. Buffer ampliado p/ 256; `g_ack_len` removido.
- **Formato:** `{"direcao","throttle","seq","ok","tlm","lq_u","lq_d","rssi_u","rssi_d","snr_u","snr_d","pwr","v","pct"}`. Ver [[Protocolo USB JSON]].
- **Verificação:** JSON simulado em Python — válido (real 153 B, pior caso 175 B < 256). Sem `%f`/`%ld` (compatível com newlib-nano): `v` via `%d.%d`, `seq` como `int`.
- **Resultado:** ✅ **validado em bancada.** ACK recebido no PC com telemetria completa:
  ```json
  {"direcao":1800,"throttle":1500,"seq":2241,"ok":1,"tlm":1,"lq_u":100,"lq_d":100,"rssi_u":-42,"rssi_d":-42,"snr_u":14,"snr_d":13,"pwr":250,"v":6.7,"pct":0}
  ```
  Eco do comando OK, telemetria coerente. **Pipeline Ranger Nano → STM32 → PC completo.**

### 2026-06-25 — Parse estruturado de 0x08 e 0x3A VALIDADO ✅
- **Arquivos:** `Core/Src/crsf.c`, `Core/Inc/crsf.h`.
- **0x08 Battery** → `[BATT] 5.8V 0.0A 0mAh 0%` (struct `g_batt`).
- **0x3A Timing** → `[TIME] interval=6666.0 us (150 Hz) offset=... us` (struct `g_timing`; header estendido tratado).
- **Resultado:** ✅ os três tipos decodificam corretamente e ao vivo.
- **Observação importante — offset do timing oscila muito** (−6040 a +4161 µs) enquanto interval fica 6666 µs:
  - Confirma que o **TX não está sincronizado em fase** com a janela de RF do módulo.
  - Agravado por `CRSF_RATE_MS = 1000/150 = 6` (truncado → ~166 Hz, mais rápido que os 150 Hz do módulo) + laço `osDelay` livre + jitter do debug bloqueante.
  - **Não quebra nada**: LQ 100/100, link sólido. É só latência não-ótima.
  - Candidato a **ADR-005 (sincronismo de fase)**: usar o `offset` p/ ajustar o tempo do envio até tender a zero. Opcional.

### 2026-06-25 — Parse estruturado do 0x14 Link Statistics (sem bancada)
- **Arquivos:** `Core/Src/crsf.c`, `Core/Inc/crsf.h`.
- **O quê:** dispatcher por tipo no parser; `crsf_decode_link()` extrai os 10 campos do `0x14` para o struct `g_link` (RSSI como `int8`, potência via tabela enum→mW) e loga na USART2:
  ```
  [LINK] LQ up=100% dn=100% | RSSI a1=-30 a2=0 dn=-40 dBm | SNR up=14 dn=13 dB | ant0 pwr=250mW mode24
  ```
- **Flags:** `CRSF_RX_DEBUG=1`; `CRSF_RX_DUMP_OTHERS=0` (silencia 0x3A/0x08 p/ focar no 0x14); struct `g_link` pronto p/ exportar via USB depois.
- **Verificação:** decode simulado em Python com o frame real → saída idêntica à esperada. Sem toolchain ARM → compilar/gravar na bancada.
- **Nota:** a linha [LINK] é longa (~8 ms bloqueante na USART2) e pode passar de um ciclo de 150 Hz → leve jitter ocasional no TX (aceitável em debug; some quando formos pro USB).
- **Resultado:** ✅ **validado em bancada.** Valores coerentes e **vivos** (RSSI variando -28..-29 up / -33..-37 dn dBm; LQ 100/100; SNR ~14/13). Confirma a leitura de RSSI como `int8`.

### 2026-06-25 — RX-com-dump VALIDADO em bancada ✅
- **Resultado:** ✅ recepção funcionando, **todos os frames com CRC OK** (nenhum ERR). Saída na USART2:
  ```
  [RX] type=0x3A len=13 crc=OK
  [RX] type=0x14 len=12 crc=OK
  [RX] type=0x08 len=10 crc=OK
  ```
- **Novo frame descoberto:** `0x08` Battery Sensor — **não** tinha aparecido no analisador. O dump no próprio MCU revelou mais que o analisador.
- **Ordem real:** `0x3A`/`0x14`/`0x08` vêm **intercalados** (não é fixo `0x14`→`0x3A`; aquilo era recorte da captura curta). Frequência aprox. observada: `0x3A` > `0x14` > `0x08`.
- **Lição (importante):** a "regressão" anterior (link caía, sem `[RX]`) **não era bug de firmware** — era o **analisador lógico carregando a linha single-wire** (capacitância/stub) e degradando as bordas a 400 kbaud quando o STM32 passou a também receber. Removida a ponta do analisador → tudo funcionou. O código de RX estava correto desde o início.
- **Próximo:** parse estruturado de `0x14` (RSSI **int8**, LQ, SNR, potência), `0x08` (bateria) e `0x3A` (timing); expor via USB.

### 2026-06-25 — Implementação do RX-com-dump (sem bancada)
- **Arquivos:** `Core/Src/crsf.c`, `Core/Inc/crsf.h`, `Core/Src/stm32f1xx_it.c`.
- **O quê:** RX por interrupção RXNE + ring buffer + parser (SYNC→LEN→DATA, valida len+CRC) + log `[RX] type=.. len=.. crc=..` na USART2. Ver [[ADR-004 Recepção de Telemetria (modo dump)]].
- **Verificação estática:** macros CMSIS (`USART_SR_*`) e HAL (`UART_IT_RXNE`, `__HAL_UART_ENABLE_IT`) confirmados; lógica do parser simulada em Python contra os frames reais (0x14/0x3A OK, corrompido → ERR). Sem toolchain ARM no ambiente → **compilar/gravar em bancada**.
- **Esperado na bancada:** `[RX] type=0x14 ... crc=OK` e `[RX] type=0x3A ... crc=OK` a ~4,8 Hz, no terminal da USART2.
- **Resultado:** ⚠️ a validar em hardware.

### 2026-06-25 — Captura no analisador lógico (TX e 1º frame de telemetria)
- **Setup:** analisador lógico em PA9 (linha CRSF half-duplex), 400 kbaud, não-invertido.
- **Captura 1 (saída do STM32):** `EE 18 16 ...` + CRC `AD` ✓ — RC Channels, 16 canais todos = 992 (1500 µs / neutro). Confirma framing, empacotamento 11-bit e CRC do TX.
- **Captura 2 (retorno do Ranger Nano):** `C8 0D 3A EA EE 10 00 01 04 64 00 00 72 C5 8F` ✓ — **0x3A.0x10 Timing Correction**: interval 6666 µs (150,02 Hz), offset +2938 µs. Origem 0xEE → destino 0xEA. Ver [[Telemetria CRSF (RX)]].
- **Captura 3 (retorno do Ranger Nano):** `C8 0C 14 E2 00 64 0E 00 18 07 D8 64 0D DF` ✓ — **0x14 Link Statistics**: LQ 100%/100%, SNR +14/+13 dB, RSSI −30/−40 dBm (int8), RF power ~250 mW, rf_mode idx 24. Ver [[Telemetria CRSF (RX)]].
- **Cadência medida:** par fixo `0x14`→`0x3A` (9,68 ms entre eles), `0x3A`→próximo `0x14` = 198,66 ms → período 208,34 ms (~4,80 Hz). Downlink com clock próprio (não múltiplo de 6,67 ms).
- **Conclusões:** linha não-invertida a 400k OK; endereçamento confirmado; módulo no perfil 150 fps (= `CRSF_RATE_HZ`); módulo pede sincronismo de fase (RC-sync); **RSSI vem como int8 com sinal** (não dBm×−1); telemetria esparsa em pares a ~4,8 Hz.
- **Próximo:** capturar eventuais outros frames (device info 0x29, menu) e/ou implementar RX-com-dump + parse de 0x14 e 0x3A.

### 2026-06-25 — Correções de tempo real na CRSF_task (sem bancada)
- **Arquivos:** `Core/Src/crsf.c`, `Core/Inc/crsf.h`
- **Mudanças:**
  - Failsafe: `CRSF_COMM_TIMEOUT_MS` 50000 → **1000** (1 s) + comentário alinhado.
  - TX CRSF: removido o busy-wait pós-DMA; agora `ulTaskNotifyTake` + `vTaskNotifyGiveFromISR` no `TxCpltCallback`.
  - Logs: `DBG_FMT` por ciclo → throttled a ~1 Hz.
- **Verificação estática:** APIs de notificação presentes na lib; `INCLUDE_xTaskGetCurrentTaskHandle=1`, `configUSE_MUTEXES=1`; IRQ DMA (prio 5) dentro de `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5). Sem toolchain ARM no ambiente → **build/flash pendente em bancada**.
- **Resultado:** ✅ **validado em bancada (2026-06-25).** Compilou, gravou e rodou sem problemas com as três mudanças (timeout 1 s, notificação de TX, log a ~1 Hz).
- **Próximos passos:** confirmar endereço CRSF esperado pelo módulo (`0xEE` vs `0xC8`) e integração RF completa (item 5 do [[Plano de Testes]]); push das alterações para o GitHub.

### 2026-06-25 — Criação do vault (sem bancada)
- **Firmware/commit:** estado inicial importado (`Radio_Master.zip`)
- **Objetivo:** registrar baseline do projeto antes dos testes.
- **Resultado:** ⚠️ ainda não testado em hardware.
- **Observações:** ver pendências em [[Questões em Aberto]] antes da primeira bancada — em especial o timeout de [[ADR-003 Estratégia de Failsafe|failsafe]].

## Relacionadas
- [[Plano de Testes]]
