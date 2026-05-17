# StarBoy — Project Context for Claude Code

## Пользователь

- **Имя:** Niko, язык: русский (код/коммиты/имена — английский)
- **ОС:** Ubuntu 24.04 LTS
- **Опыт:** Arduino-любитель, первый серьёзный embedded-проект
- **Стиль:** короткие вопросы перед сложным, конкретные команды для копипасты, честная обратная связь

## Проект

**StarBoy** — носимый AI-аксессуар (брелок/кулон) с круглым AMOLED, на котором живут анимированные глазки. Слушает голос, отвечает через облачный AI, реагирует на движение. Эстетика: «Obsidian Mirror» — чёрный обсидиан, амбер-glow в глазах. Первый прототип — 8-конечная звезда, ~95мм.

## Железо

**Waveshare ESP32-S3-Touch-AMOLED-1.32**
- ESP32-S3-PICO-1-N8R8: 2× Xtensa LX7 240МГц, 8МБ PSRAM, 8МБ Flash
- Wi-Fi 2.4GHz, BT5 LE
- AMOLED 1.32" 466×466, контроллер CO5300 (QSPI)
- Touch CST820 (I2C), аудиокодек ES8311 (I2S), встроенный микрофон
- Питание Li-Ion 3.7V (MX1.25), динамик 8Ω 2W

### Распиновка
```
Display CO5300 (QSPI): RESET=GPIO8, TE=GPIO9, CS=GPIO10, SCL=GPIO11, D0-3=GPIO12-15
Touch CST820 (I2C):    INT=GPIO6, RST=GPIO7, SDA=GPIO47, SCL=GPIO48
Audio ES8311 (I2S):    ASDOUT=GPIO40, LRCK=GPIO41, DSDIN=GPIO42, MCLK=GPIO38, SCLK=GPIO39, PA_CTRL=GPIO46
                       Codec I2C: SDA=GPIO47, SCL=GPIO48 (общая шина с touch)
Кнопки:               PWR=GPIO17, BOOT=GPIO0
Свободные GPIO:        GPIO0, GPIO1, GPIO2 (на 12-pin разъёме SH1.0)
```

### Докупаем
- Гироскоп: LSM6DS3 или MPU6050 (I2C, GPIO47/48)
- Динамик 8Ω 2W, MX1.25
- LiPo 3.7V, MX1.25
- Камера OV2640 — отложена (Этап 7+)

## Dev-стек

- VS Code (.deb, **не snap** — snap ломает PlatformIO) + PlatformIO IDE
- Arduino framework через PlatformIO (переход на ESP-IDF только по явному запросу)
- Путь проекта: `/home/niko/code/LILBRO/starboy-firmware/`
- Linux: пользователь в группах `dialout`, `plugdev`, udev-правила PlatformIO установлены

## Архитектура

- **AI:** wake-word локально (`esp-sr`), диалог через облако: mic→I2S→ESP32→Wi-Fi→STT→LLM→TTS→I2S→speaker
- **FreeRTOS:** display/audio/network — отдельные задачи, не blocking main loop
- **PSRAM обязательно** для frame buffer (466×466×2 = ~434КБ)
- Личность = системный промпт LLM, no fine-tuning

## Принципы кода

- Каждая подсистема — отдельная папка/класс (display, eyes, audio, sensors, wifi, ai, ble, power)
- Секреты — в `secrets.h` (в `.gitignore`), в репо только `secrets.example.h`
- Не оптимизировать преждевременно: MVP → профилирование → оптимизация
- Не писать код «впрок» — строго то, что нужно сейчас

## Чего НЕ делать без подтверждения

- Закупать железо (давать BOM, Niko сам заказывает)
- Выбирать AI-провайдера (Claude vs GPT vs Gemini)
- Переходить на ESP-IDF
- Добавлять библиотеки без обоснования
- Трогать дизайн корпуса (отложен)
