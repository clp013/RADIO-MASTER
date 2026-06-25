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

### 2026-06-25 — Parse estruturado de 0x08 e 0x3A VALIDADO ✅
- **Arquivos:** `Core/Src/crsf.c`, `Core/Inc/crsf.h`.
- **0x08 Battery** → `[BATT] 5.8V 0.0A 0mAh 0%` (struct `g_batt`).
- **0x3A Timing** → `[TIME] interval=6666.0 us (150 Hz) offset=... us` (struct `g_timing`; header estendido tratado).
- **Resultado:** ✅ os três tipos decodificam corretamente e ao vivo.
- **Observação importante — offset do timing oscila muito** (