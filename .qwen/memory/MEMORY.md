# ЖЁСТКОЕ ПРАВИЛО — ПИСАТЬ ТОЛЬКО СЮДА
# Путь проектной памяти: /home/enclave/src/Enclave_OS_arm/.qwen/memory/
# НЕ писать в /home/enclave/.qwen/memories/ (USER memory),
# НЕ писать в /home/enclave/.qwen/projects/ (это каталог кросс-проектов, не этот проект),
# НЕ писать в QWEN.md — он 30-строчная заглушка.
# ---

- [Оркестратор ОБЯЗАН делегировать субагентам, не решать сам](reference/auto-delegation.md) — project
- [Архитектура и инварианты BCM2835/ARMv6](architecture.md) — Векторы на 0x0, стек всех режимов, адреса периферии, 8 инвариантов, правила кода
- [Текущее состояние проекта: вехи M0–M8](context.md) — Завершено M0–M2, в работе правки, в планах прерывания/MMU/анклав
- [История решений — почему сделано именно так](decisions.md) — Selftest в образе, 3 состояния PASS/FAIL/SKIP, ATAG-границы, ELF low memory, роли субагентов
- [Известные баги и обходные пути](bugs.md) — Serial stdio в pipe, двойной loader, ELF на 0x8000, пробелы вместо табов, ATAG номер
- [Частые паттерны ошибок MMIO и UART](patterns.md) — volatile для MMIO, ожидание TXFF, выравнивание стека, subs pc/lr, implicit declaration, не-ASCII
