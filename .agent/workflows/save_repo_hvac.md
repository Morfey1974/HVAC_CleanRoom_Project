---
name: save_repo_hvac
description: "Saves the HVAC project locally and pushes to the remote repository."
---

# save_repo_hvac

Эта команда автоматизирует процесс сохранения (commit & push) проекта `HVAC_CleanRoom_Project`.

**Правила выполнения команды:**
1. Перейдите в корневую директорию проекта: `c:\Users\morfe\STM32CubeIDE\workspace_1.17.0\HVAC_CleanRoom_Project`.
2. Добавьте все изменения: `git add .`
3. Сделайте коммит с автоматическим сообщением. Если комментария нет, используйте `git commit -m "Auto-save HVAC Project"`.
4. Отправьте изменения в удаленный репозиторий: `git push origin main`.
5. Сообщите пользователю об успешном сохранении.
