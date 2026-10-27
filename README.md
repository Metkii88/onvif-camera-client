# ONVIF Camera Client (Astra Linux 1.8)

Клиент IP-камер: WS-Discovery (ONVIF) поиск в локальной сети, просмотр нескольких RTSP-потоков,
сохранённые профили камер, полноэкранный режим, установка в систему с иконкой.

## Возможности (v2.0)
- **Поиск ONVIF-камер** по сети (WS-Discovery) + ручной шаблон добавления: адрес/порт, логин, пароль, путь RTSP.
- **Хранилище настроенных камер**: сохранение в `~/.config/OnvifClient/cameras.json`, выбор из списка, редактирование/удаление, подключение в один клик.
- **Многопоточный просмотр**: сетка ячеек 1/2/3/4 потоков одновременно.
- **Полноэкранный режим**: F11 — войти/выйти, Esc — выйти из полного экрана; запуск сразу в полный экран: `--fullscreen`.
- **Запуск с профилем**: `onvif-client --profile "Двор,Вход"` подключает сохранённые камеры автоматически.
- **Установка в систему**: `sudo ./install.sh` кладёт бинарник в `/usr/local/bin`, desktop-файл в меню («Камеры ONVIF») и иконку камеры.

## Сборка
```bash
apt install qtbase5-dev libqt5widgets5 pkg-config cmake g++ \
            libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

## Запуск
```bash
./build/onvif-client                       # обычный режим
./build/onvif-client -f                    # полноэкранный
./build/onvif-client -p "Камера1,Камера2" -f
```

## Установка релизного пакета
```bash
tar xzf onvif-camera-client-2.0.0-astra1.8-x86_64.tar.gz
cd onvif-camera-client-2.0.0 && sudo ./install.sh
```
Зависимости из репозитория Astra: `libqt5widgets5 libqt5network5 libavformat* libavcodec* libavutil* libswscale*`.

## Горячие клавиши
| Клавиша | Действие |
|---|---|
| F11 | Полноэкранный режим (вкл/выкл) |
| Esc | Выход из полноэкранного режима |
