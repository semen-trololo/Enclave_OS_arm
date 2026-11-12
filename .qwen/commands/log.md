---
description: Анализ логов сборки (модель — субагент logger)
---

Сам лог не разбирай. Запусти инструмент `agent` c `subagent_type: logger`, передав в `prompt` лог целиком, без сокращений.

Если logger вернул `[ESCALATE]` → запусти `agent` c `subagent_type: debugger` с этим же логом и причиной эскалации.

Если logger вернул `[PATTERN]` и исправление подтверждено сборкой → вызови /remember-pattern.

Лог: {{args}}
