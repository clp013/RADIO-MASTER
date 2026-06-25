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

### 2026-06-25 — RX-com-dump VALIDADO em bancada ✅
- **Resultado:** ✅ recepção funcionando, **todos os frames com CRC OK** (nenhum ERR). Saída na USART2:
  ```
  [RX] type=0x3A len=13 crc=OK
  [RX] type=0x14 len=12 crc=OK
  [RX] type=0x08 len=10 crc=OK
  ```
- **Novo frame descoberto:** `0x08` Battery Sensor — **não** tinha aparecido no analisador. O dump no próprio MCU revelou mais que o analisador.
- **Ordem real:** `0x3A`/`0x