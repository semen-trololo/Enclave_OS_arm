# ЖЁСТКОЕ ПРАВИЛО — ПИСАТЬ ТОЛЬКО СЮДА
# Путь проектной памяти: <репо>/.qwen/memory/
# НЕ писать в ~/.qwen/memories/ (USER memory), НЕ писать в ~/.qwen/projects/,
# НЕ писать в QWEN.md — зона человека.

Порядок чтения: QWEN.md (правила) → README.md (состояние) → architecture → context → bugs.

- architecture.md — BCM2835/ARMv6: карта памяти, IC/таймер/mailbox/UART/GPIO, кадр
  исключений, инварианты, различия QEMU/плата
- context.md — текущее состояние: M0–M4 завершены (QEMU+плата, логи 15.11, коммит 767f0bf),
  дальше M4.5/M5
- bugs.md — активные обходные пути + архив исправленного (с доказательствами)
- decisions.md — почему сделано именно так (IC не PL190, C1 не SP804, soft-float, ELF/raw)
- patterns.md — грабли MMIO/UART/типов/сборки и правила промтов coder'у
- delegate-to-coder.md — шаблон делегирования coder'у (актуальный, без шага коммита)
