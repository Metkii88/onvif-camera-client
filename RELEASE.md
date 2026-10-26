# Релиз 1.0.0 — ONVIF Camera Client для Astra Linux 1.8

Готовый самодостаточный релизный пакет: **`onvif-camera-client-1.0.0-astra1.8-x86_64.tar.gz`** (78 МБ).

## Состав пакета
```
onvif-camera-client-1.0.0/
├── run.sh          — точка входа (настраивает LD_LIBRARY_PATH и QT_PLUGIN_PATH)
├── bin/onvif-client— исполняемый файл
├── lib/            — все зависимости (Qt 5.15, FFmpeg/libav, X11/xcb) — ~170 .so
├── plugins/platforms/ — Qt-плаформы xcb и offscreen
└── README.txt      — краткая инструкция
```

## Возможности
* **ONVIF / WS-Discovery** — поиск камер в локальной сети (UDP multicast 239.255.255.250:3702)
* Ручное добавление камеры по RTSP URL
* **Мультипросмотр до 4 потоков** (сетки 1×1 / 2×2), декодирование через FFmpeg libav

## Установка и запуск на Astra Linux 1.8 (x86_64)
```bash
tar xzf onvif-camera-client-1.0.0-astra1.8-x86_64.tar.gz
cd onvif-camera-client-1.0.0
./run.sh
```
Системные Qt/FFmpeg **не требуются** — всё включено в пакет. Нужны только базовые
системные библиотеки (glibc, X11), входящие в Astra 1.8. Для работы видеопотоков
в СБД/мандольном окружении убедитесь, что сессия имеет доступ к X-серверу (fly/FLY WM).

## Сборка из исходников
```bash
sudo apt install qtbase5-dev libqt5network5 cmake pkg-config \
     libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
cmake -B build && cmake --build build -j
```

Контрольная сумма MD5 архива указана в теле release.
