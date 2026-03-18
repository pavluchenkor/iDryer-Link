# Порядок запросов к Moonraker

## 1. Проверяем, идёт ли печать и получаем имя файла

Запрос:

```
GET /printer/objects/query?print_stats=state,filename
```

Полный URL:

```
http://192.168.1.21:7125/printer/objects/query?print_stats=state,filename
```

Ответ (пример):

```json
{
  "result": {
    "status": {
      "print_stats": {
        "state": "printing",
        "filename": ".cache/Куб_ABS_11m29s.gcode"
      }
    }
  }
}
```

Нужные поля:

```
result.status.print_stats.state
result.status.print_stats.filename
```

---

## 2. Если `state == printing`, запрашиваем metadata файла

Запрос:

```
GET /server/files/metadata?filename=.cache/Куб_ABS_11m29s.gcode
```

Полный URL:

```
http://192.168.1.21:7125/server/files/metadata?filename=.cache/Куб_ABS_11m29s.gcode
```

Ответ (пример):

```json
{
  "result": {
    "chamber_temp": 60.0,
    "filament_type": "ABS;ABS"
  }
}
```

Нужные поля:

```
result.chamber_temp
result.filament_type
```

---

## 3. Логика работы

1. Каждые **10–30 секунд** проверяем статус печати:

```
/printer/objects/query?print_stats=state,filename
```

2. Если:

```
state == printing
```

3. Получаем:

```
filename
```

4. Один раз запрашиваем metadata:

```
/server/files/metadata?filename=...
```

5. Используем:

```
chamber_temp
filament_type
```

---

## 4. Использование для управления камерой

```
if chamber_temp > 0
    включить нагрев камеры
else
    не включать
```

`filament_type` можно использовать как дополнительный признак материала.
