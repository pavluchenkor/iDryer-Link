# Post-Build Scripts

Документация по скриптам автоматического копирования прошивок после сборки.

## ESP32 (iDryer Link)

**Файл:** `extra_scripts/copy_firmware.py`

**Назначение:** Копирует прошивку ESP32 (firmware.bin + bootloader.bin + partitions.bin + boot_app0.bin) в две папки после каждой сборки.

**Пути назначения:**
1. Локально: `firmware/<board>/`
2. Flasher Portal: `/Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/link/<slot>/<board>/`

**Маппинг плат:**
- `esp32c3-prod` → `esp32c3` (flasher-portal)
- `esp32c3-super-mini-prod` → `esp32c3-super-mini` (flasher-portal)
- `xiao-esp32s3-prod` → `xiao-esp32s3` (flasher-portal)
- `waveshare-esp32s3-zero-prod` → `waveshare-esp32s3-zero` (flasher-portal)

**Пример вывода:**
```
[FIRMWARE] Copying esp32c3-prod firmware...
[FIRMWARE] firmware.bin → firmware/esp32c3/firmware.bin
[FIRMWARE] firmware.bin → /Users/.../flasher-portal/firmware/link/prod/esp32c3/firmware.bin
[FIRMWARE] ✅ esp32c3-prod copied successfully!
```

## RP2040 (iDryer Controller)

**Файл:** `scripts/post_build.py`

**Назначение:** Копирует прошивку RP2040 (firmware.uf2) в две папки после каждой сборки.

**Пути назначения:**
1. Локально: `firmware/`
2. Flasher Portal: `/Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/rp2040/`

**Формат имен файлов:**
- `firmware_{sht31_addr}_v{version}.uf2` (если задан SHT31_ADDRESS)
- `firmware_v{version}.uf2` (если SHT31_ADDRESS не задан)

**Пример вывода:**
```
[RP2040] Copying firmware_0x44_v1.0.0.uf2...
[RP2040] firmware_0x44_v1.0.0.uf2 → firmware/firmware_0x44_v1.0.0.uf2
[RP2040] firmware_0x44_v1.0.0.uf2 → /Users/.../flasher-portal/firmware/rp2040/firmware_0x44_v1.0.0.uf2
[RP2040] ✅ Firmware copied successfully!
```

## Настройка для разработчиков

### Автоматическое копирование в flasher-portal

Если вы разработчик проекта и используете flasher-portal, настройте переменную окружения:

**Linux/macOS:**
```bash
# Добавьте в ~/.bashrc или ~/.zshrc
export IDRYER_FLASHER_PORTAL_PATH="/path/to/your/iDryerPortal/flasher-portal"
```

**Windows:**
```cmd
set IDRYER_FLASHER_PORTAL_PATH=C:\path\to\your\iDryerPortal\flasher-portal
```

### Результат работы

**Без переменной окружения (обычные пользователи):**
```
[FIRMWARE] ✅ esp32c3-prod copied to local!
```

**С переменной окружения (разработчики):**
```
[FIRMWARE] ✅ esp32c3-prod copied to local + flasher-portal!
```

## Обновления (2026-03-25)

✅ **ESP32:** Добавлено условное копирование через переменную окружения
✅ **RP2040:** Добавлено условное копирование через переменную окружения  
✅ **Оба:** Добавлены цветные уведомления об успешном копировании
✅ **GitHub-ready:** Скрипты безопасны для публикации, никаких хардкод путей

## Статус

- ✅ ESP32 скрипт работает корректно
- ✅ RP2040 скрипт работает корректно  
- ✅ Безопасно для публикации на GitHub
- ✅ Автоматическое определение наличия flasher-portal
- ✅ Цветные уведомления отображаются в терминале