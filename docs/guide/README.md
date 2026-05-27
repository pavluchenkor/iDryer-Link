# Репозиторий iDryer Link — справка

**iDryer Link** — готовый продукт (железо + прошивка модуля). Программист может использовать этот репозиторий как **образец сборки и пинов**, но **нормативная документация для стороннего устройства** живёт в библиотеке **`idryer-core`**.

## Разработчик своего продукта в облаке iDryer

Документация протокола — в [lib/idryer-core/docs/ru/README.md](../../lib/idryer-core/docs/ru/README.md) (или другой язык).

Контракт обмена (MQTT + UART) — в [lib/idryer-core/contracts/mqtt_contract.yaml](../../lib/idryer-core/contracts/mqtt_contract.yaml).

## Документы именно этого репозитория

| Файл | Для кого |
|------|----------|
| [docs/README.ru.md](../README.ru.md) · [README.en.md](../README.en.md) | Пользователь: подключение, веб-флешер |
| [STAGING.md](../developer/STAGING.md) | Разработчик Link: тестовый стенд, auto-claim |
| [POST_BUILD_SCRIPTS.md](../developer/POST_BUILD_SCRIPTS.md) | Сборка → flasher-portal |
| [TOOLS.md](../developer/TOOLS.md), [tools/README.md](../../tools/README.md) | Эмулятор MCU, mock API |
| [platformio.ini](../../platformio.ini) | Окружения, `IDRYER_*`, MQTT |
| [on-security-trust-and-open-source.md](../../on-security-trust-and-open-source.md) | Доверие и открытый код |
