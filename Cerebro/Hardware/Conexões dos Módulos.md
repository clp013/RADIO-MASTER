---
title: Conexões dos Módulos
tags:
  - hardware
  - conexoes
created: 2026-06-25
---

# Conexões dos Módulos

## Diagrama de ligações
```mermaid
graph TD
    subgraph Blue Pill (STM32F103C8)
        PA9["PA9 — USART1 TX/RX (half-duplex)"]
        PA2["PA2 — USART2 TX"]
        USB["USB (PA11/PA12)"]
        GND1["GND"]
    end
    PA9 ---|sinal CRSF 400k| MOD["Módulo Crossfire/ELRS<br/>(pino CRSF/Sinal)"]
    PA2 ---|texto 115200| SER["Adaptador USB-Serial (RX)"]
    USB ---|cabo USB| PC["Host / PC"]
    GND1 --- GNDC["GND comum: módulo + serial + PC"]
```

## Tabela de conexões

| De (STM32) | Para | Observação |
|------------|------|------------|
| PA9 (USART1) | Pino de sinal CRSF do módulo RF | Um único fio (half-duplex). Verificar nível lógico 3.3 V |
| PA2 (USART2 TX) | RX do adaptador USB-Serial | Debug em [[Debug UART]] |
| USB (PA11/PA12) | PC | Porta serial virtual + alimentação |
| GND | GND do módulo e do serial | **GND comum obrigatório** |

> [!warning] Pontos a confirmar em bancada
> - **Polaridade/nível do sinal CRSF** esperado pelo módulo (não-invertido, 3.3 V).
> - Se o módulo exige **5 V** na linha de sinal → pode precisar de level shifter.
> - Alimentação do módulo (separada do USB da Blue Pill) com GND unido.
> - Registrar a pinagem física exata do conector do módulo em [[Log de Testes]].

## Relacionadas
- [[Pinout STM32F103C8]]
- [[Protocolo CRSF]]
- [[Questões em Aberto]]
