---
description: Сложный дебаг и архитектура (модель — субагент debugger)
---

Сам не рассуждай. Запусти инструмент `agent` c `subagent_type: debugger`, передав в `prompt`: задачу, причину эскалации дословно, вывод сборки/дампа и имена затронутых файлов.

После ответа debugger:

- `[BUG_FIXED]` → вызови /remember-bug
- `[DECISION]` → вызови /remember-decision

Задача: {{args}}
