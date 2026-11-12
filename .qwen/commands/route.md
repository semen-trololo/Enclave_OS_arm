---
description: Классификатор задач (модель роутера — субагент router)
---

Сам не классифицируй. Запусти инструмент `agent` c `subagent_type: router` и полным текстом запроса ниже — он вернёт одно слово.

Полученное слово выполни, не переспрашивая:

- `/code` → запусти `agent` c `subagent_type: coder`
- `/log` → запусти `agent` c `subagent_type: logger`
- `/debug` → запусти `agent` c `subagent_type: debugger`
- `/answer` → ответь сам, только по файлам из `.qwen/memory/`

Запрос: {{args}}
