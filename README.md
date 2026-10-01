# ESP-HACK v1.3-s3 — порт на LilyGO T-Display-S3 (LCD, 2 кнопки)

Порт [ESP-HACK](https://github.com/Teapot174/ESP-HACK) с ESP32-WROOM-32 + OLED 128x64
на LilyGO **T-Display-S3** (версия **LCD**, ESP32-S3R8, 16 МБ flash, 8 МБ OPI PSRAM).

## Управление (две кнопки вместо четырёх)

| Кнопка | Короткое нажатие | Удержание (≥ 400 мс) | Очень долгое (≥ 2 с) |
|---|---|---|---|
| **BTN1** (GPIO0) | **UP** — листание вверх | **OK** — принять | дополнительно `isHolded()` |
| **BTN2** (GPIO14) | **DOWN** — листание вниз | **BACK** — назад | дополнительно `isHolded()` |

Аккорды не используются: каждая кнопка делится по длительности удержания. Тап решается
в момент отпускания, удержание — пока кнопка нажата (иначе `GyverButton` снимет флаг
клика и «принять» не сработает).

## Дисплей

ST7789 170x320 на 8-битной параллельной шине. Логический холст ESP-HACK сохранён как 128x64
и растягивается на всю панель неоднородным целочисленным масштабом (2x/3x по каждой оси),
поэтому чёрных рамок и мёртвых зон нет, а интерфейс рисуется без правок.

## Сборка

```powershell
$env:PYTHONUTF8 = '1'
python -m platformio run -e tdisplay-s3
python -m platformio run -e tdisplay-s3 -t upload
```

## Сброс после прошивки

На встроенном USB-Serial/JTAG `--after hard-reset` возвращает чип в режим загрузки.
Чтобы запустить приложение:

```powershell
python -m esptool --chip esp32s3 --port COM17 --before default-reset --after watchdog-reset read-mac
```

## Структура изменений

* `src/CONFIG.h` — новая распиновка под ESP32-S3R8 (GPIO3 намеренно не занят: стрэп-вывод JTAG).
* `src/tds3_display.{h,cpp}` — адаптер ST7789 с интерфейсом Adafruit OLED.
* `src/tds3_buttons.{h,cpp}` + `src/esp_hack_shim.h` — две кнопки → четыре логические.
* `src/GyverButton.{h,cpp}` — вендорена, чтобы её `digitalRead` шёл через шим.
* `src/display.h` — `DisplayType` → `TDS3_Display`, псевдонимы `SH110X_*`.
* `src/ble_spam.{h,cpp}`, `src/bluetooth.cpp` — миграция NimBLE 1.x → 2.x (на S3 нет Bluedroid).
* `lib/OneWireHub/src/platform.h` — `#undef MEM_SIZE` (конфликт с lwIP).
* `build.py`, `partitions_tds3.csv`, `platformio.ini` — под ESP32-S3 и 16 МБ flash.

Подробности — в `PORTING-NOTES-ru.md`.
