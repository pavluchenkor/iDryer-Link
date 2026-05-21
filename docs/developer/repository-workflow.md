# Две ветки и два репозитория

**Документация для разработчика стороннего устройства** (библиотека `idryer-protocol`, облако iDryer): [lib/idryer-protocol/docs/00-developer/01-your-product-in-idryer-cloud.md](../../lib/idryer-protocol/docs/00-developer/01-your-product-in-idryer-cloud.md). Справка по репозиторию Link: [docs/guide/README.md](../guide/README.md).

---

Цель: все подробные коммиты живут в закрытом репозитории (`private`) в ветке `dev`; на GitHub (`origin`) публикуется только аккуратный `main`.

## Разовая настройка
- Проверить текущую ветку: `git status -sb` или точечно `git branch --show-current`.
- Если база называлась `master`, переименовать: `git branch -m master dev`.
- Убедиться, что закрытый remote уже есть: `git remote -v` (ищем `private`).
- Создать чистый `main` из текущего состояния:
  ```bash
  git checkout dev
  git checkout --orphan main
  git add .
  git commit -m "feat: initial clean release"
  git push origin main      # только чистая ветка
  git push private dev      # полный журнал в приват
  git branch --set-upstream-to=private/dev dev
  git branch --set-upstream-to=origin/main main
  ```

## Как создать репозитории через GitHub CLI (`gh`)
- Публичный: `gh repo create <user>/<repo> --public --source . --remote origin --push`.
- Закрытый: `gh repo create <user>/<repo>-private --private --source . --remote private --push`.
- Если репозитории уже созданы в веб-интерфейсе, просто добавьте remotes:  
  `git remote add origin git@github.com:<user>/<repo>.git`  
  `git remote add private git@github.com:<user>/<repo>-private.git`

## Ежедневная работа (dev)
- Всегда коммитьте в `dev`.
- Бэкапите историю только в закрытый remote: `git push private dev`.
- Не пушьте `dev` в `origin`, чтобы не раскрывать процесс.

## Публикация чистой версии (main)
```bash
git checkout main
git merge --squash dev   # берём результат без истории
git commit -m "feat: краткое описание релиза"
git push origin main     # видно на GitHub
git checkout dev
git merge main           # опционально подтянуть итоговый снапшот
git push private dev     # обновить приватную историю
```

## Быстрые проверки
- `git status` — нет ли случайных изменений.
- `git log --oneline --decorate --graph --all` — `dev` уходит только в `private`, `main` линейный.

## Резервная копия офлайн
- `git bundle create backups/dev-$(date +%F).bundle dev`
