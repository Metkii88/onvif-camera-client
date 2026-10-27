# ONVIF Camera Client v2.0 (Astra Linux 1.8)

Клиент видеонаблюдения: поиск IP-камер по **ONVIF / WS-Discovery** в локальной сети, шаблон ручного добавления камеры (IP/логин/пароль), сохранение настроенных камер и вывод до **18 RTSP-потоков** на экран (пресеты сетки: 1×1, 2×2, 3×3, 4×4, 3×6, 6×3). Стек: Qt 5 (Widgets, Network) + FFmpeg (libav).

## Что нового в v2.0

- 📝 **Шаблон поиска/добавления камеры с параметрами**: адрес (IP/хост), порт, логин, пароль, путь RTSP — подключение камер без ONVIF-discovery (диалог «Добавить камеру»).
- 💾 **Хранилище настроенных камер**: `~/.config/OnvifClient/cameras.json` (пароли обфусцированы base64). Вкладка «Сохранённые камеры» — выбор, подключение, редактирование, удаление.
- ⛶ **Полноэкранный режим**: `F11` — включить/выключить, `Esc` — выход из полного экрана.
- 🚀 **Запуск с заданным профилем**: `--profile "Двор,Вход" --fullscreen` — приложение сразу подключает сохранённые камеры, подбирает сетку под их число и разворачивается на весь экран.
- 🎨 **Установка в систему**: `install.sh` — бинарник в `/usr/local/bin`, пункт меню `.desktop` и **иконка камеры** (SVG + PNG 16–256 px) в тему иконок.

## Быстрый старт (релизный пакет)

Скачайте `onvif-camera-client-2.0.0-astra1.8-x86_64.tar.gz` из [Releases](../../releases):

```bash
tar xzf onvif-camera-client-2.0.0-astra1.8-x86_64.tar.gz
cd onvif-camera-client-2.0.0
./bin/onvif-client        # запуск без установки
sudo ./install.sh         # установка в систему (можно PREFIX=/opt ...)
onvif-client              # запуск из любого места; значок в меню приложений
```

Зависимости — штатные пакеты Astra Linux 1.8:

```bash
apt install libqt5widgets5 libqt5network5 libavformat57 libavcodec57 libavutil55 libswscale4
```

## Командная строка

```
onvif-client [параметры]
  -p, --profile <имена>   Подключить сохранённые камеры при старте
                          (имена или IP; через запятую или повтор флага)
  -f, --fullscreen        Запустить в полноэкранном режиме
  -h, --help              Справка
  -v, --version           Версия
```

Примеры:

```bash
onvif-client --profile "Двор,Вход" --fullscreen
onvif-client -p Двор -p Вход -f
```

## Поиск и добавление камер

**ONVIF-поиск**: вкладка «ONVIF (multicast)» → «Начать поиск». Клиент рассылает WS-Discovery probe (`239.255.255.250:3701`) и собирает ответы камер. При подключении запрашиваются `GetProfiles`/`GetStreamUri` с учётом логина/пароля; камера автоматически сохраняется.

**Шаблон (ручной ввод)**: кнопка «Добавить камеру» — задайте адрес, порт, логин, пароль и путь RTSP. Камера подключается и сохраняется в списке для повторного использования (в т.ч. через `--profile`).

## Горячие клавиши

| Клавиша | Действие |
|---|---|
| `F11` | Переключить полноэкранный режим |
| `Esc` | Выйти из полноэкранного режима |
| `Ctrl+Q` | Выход |

## Сборка из исходников

```bash
sudo apt install qtbase5-dev cmake pkg-config \
     libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
cmake -B build && cmake --build build -j
./build/onvif-client
```

## Структура проекта

```
src/            исходники (Qt/C++): main.cpp, mainwindow.*, cameradialog.*,
                camerastore.*, camerascanner.*, onvifdiscovery.*, videocell.*, cameradecoder.*
packaging/      .desktop, SVG-иконка камеры, RPM spec
install.sh      установка в систему (/usr/local: bin, desktop-файл, иконки)
CMakeLists.txt  сборка
RELEASE.md      заметки релиза
```

## Требования

- Astra Linux 1.8 (x86_64) или совместимый Debian; рабочий стол X11/xcb (Orel/Yed)
- Доступ к камерам по ONVIF/RTSP

## Лицензия

MIT
