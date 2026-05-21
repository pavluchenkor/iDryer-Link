# Репозиторий iDryer Link — справка

**iDryer Link** — готовый продукт (железо + прошивка модуля). Программист может использовать этот репозиторий как **образец сборки и пинов**, но **нормативная документация для стороннего устройства** живёт в библиотеке **`idryer-protocol`**.

## Разработчик своего продукта в облаке iDryer

Начинайте здесь (в составе monorepo путь относительно корня `idryer-link`):

- [lib/idryer-protocol/docs/00-developer/01-your-product-in-idryer-cloud.md](../../lib/idryer-protocol/docs/00-developer/01-your-product-in-idryer-cloud.md)
- [Оглавление `docs/` библиотеки](../../lib/idryer-protocol/docs/README.md)

Дальше в каждом разделе спеки есть **`00-for-product-developers.md`** (UART, MQTT, HTTP, потоки, опции).

## Документы именно этого репозитория

| Файл | Для кого |
|------|----------|
| [docs/README.ru.md](../README.ru.md) · [README.en.md](../README.en.md) | Пользователь: подключение, веб-флешер |
| [STAGING.md](../developer/STAGING.md) | Разработчик Link: тестовый стенд, auto-claim |
| [POST_BUILD_SCRIPTS.md](../developer/POST_BUILD_SCRIPTS.md) | Сборка → flasher-portal |
| [TOOLS.md](../developer/TOOLS.md), [tools/README.md](../../tools/README.md) | Эмулятор MCU, mock API |
| [platformio.ini](../../platformio.ini) | Окружения, `IDRYER_*`, MQTT |
| [on-security-trust-and-open-source.md](../../on-security-trust-and-open-source.md) | Доверие и открытый код |

При **отдельном клоне только `idryer-protocol`** ссылки на `tools/` и `STAGING` ведите на репозиторий Link или дублируйте инструменты у себя.
