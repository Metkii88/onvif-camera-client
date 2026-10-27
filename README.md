# ONVIF Camera Client (Astra Linux 1.8)

Простой клиент видеонаблюдения: поиск камер по **ONVIF / WS-Discovery** в локальной сети и до **18 RTSP-потоков** на экран (пресеты сетки: 1×1, 2×2, 3×3, 4×4, 3×6 и 6×3 — по 18). Стек: Qt 5 (Widgets, Network) + FFmpeg (libav).

## Быстрый старт (релизный пакет)
Скачайте `onvif-camera-client-1.0.0-astra1.8-x86_64.tar.gz` из [Releases](../../releases), затем:
```bash
tar xzf onvif-camera-client-1.0.0-astra1.8-x86_64.tar.gz
cd onvif-camera-client-1.0.0 && ./run.sh
```
Все зависимости (Qt5, FFmpeg) включены в пакет — ничего дополнительно ставить не нужно.

## Сборка из исходников
```bash
sudo apt install qtbase5-dev cmake pkg-config \
     libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
cmake -B build && cmake --build build -j
./build/onvif-client
```

## Структура
* `src/onvifdiscovery.*` — WS-Discovery (UDP multicast 239.255.255.250:3702)
* `src/cameradecoder.*` — RTSP-захват/декодирование (libavformat/libavcodec/swscale)
* `src/videocell.*`, `src/mainwindow.*` — UI мультипросмотра
