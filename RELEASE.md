# Release v2.2.0 — исправленный поиск по шаблонам + UI-фиксы

## Изменения относительно v2.1
- 🔍 **Исправлен поиск камер по шаблонам**: корректная обработка диапазонов
  `192.168.0.1-254`, `192.168.0.1-30`, одиночных IP и `192.168.0` (весь /24).
  Переписан RTSP-сканер (`src/camerascanner.cpp`): параллельное сканирование,
  авторизация Basic **и Digest**, надёжный разбор ответов DESCRIBE,
  таймауты и отмена поиска.
- 📷 Значения по умолчанию в диалоге камеры: **порт 554, кодек h264,
  логин admin, пароль admin, протокол rtsp**. Добавлены типовые пути потоков
  Hikvision/Dahua/Axis: `/h264/ch1/main/av_stream`, `/media/video1`,
  `/unicast/c1/s0/live`, `/onvif1`, `/Streaming/Channels/101` и др.
- ✏️ Текст во всех полях ввода больше не обрезается рамками: увеличены
  padding и минимальная высота QLineEdit/QSpinBox/QComboBox в `themes/style.qss`.
- 🖥 **Полноэкранный режим только для сетки камер** (кнопка «⛶ Только сетка на
  весь экран», F11, выход — Esc), а не всё окно программы со всеми панелями.

## Сборка (Astra Linux 1.8 x86_64)
```bash
cmake -B build && cmake --build build -j$(nproc)
./build/onvif-client
```

## Установка
```bash
tar xzf onvif-camera-client-2.2.0-astra1.8-x86_64.tar.gz
cd onvif-camera-client-2.2.0 && ./run.sh          # или sudo ./install.sh
```
