# Cérebro RADIO_MASTER (Vault Obsidian)

Este diretório é um **vault do Obsidian** que serve de "cérebro" do projeto de firmware RADIO_MASTER (STM32F103C8T6 → CRSF).

## Como abrir
1. Instale o [Obsidian](https://obsidian.md).
2. `Open folder as vault` → selecione esta pasta `Cerebro`.
3. Comece pela nota índice: **`00 - Indice/🧠 MOC - Cérebro RADIO_MASTER`**.

## Organização
- **Contexto/** — visão geral e glossário.
- **Hardware/** — pinout, clock, conexões físicas.
- **Firmware/** — arquitetura, tasks FreeRTOS, drivers.
- **Protocolo/** — CRSF e protocolo JSON do host.
- **Decisoes Tecnicas/** — ADRs (Architecture Decision Records).
- **Testes/** — plano e log de testes.
- **Conteudo Tecnico/** — referências e questões em aberto.

## Convenções
- Notas em **português**, ligadas por `[[wikilinks]]`.
- Cada decisão importante vira um **ADR** numerado.
- Cada bancada de teste gera uma entrada datada em [[Log de Testes]].
- Dúvidas e bugs ficam em [[Questões em Aberto]] até virarem ADR ou correção.
