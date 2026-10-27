# v2.0.0

## Новое
- Шаблон ручного добавления камеры: IP/адрес, порт, логин, пароль, путь RTSP (диалог «Добавить камеру»).
- Хранилище настроенных камер (`~/.config/OnvifClient/cameras.json`) со списком, выбором, редактированием и удалением; пароли кодируются base64.
- Подключение нескольких потоков из сохранённых профилей в сетке ячеек.
- Полноэкранный режим: F11 / Esc; флаг запуска `--fullscreen` (`-f`).
- Автоподключение профилей при старте: `--profile "имя1,имя2"` (`-p`), значения через запятую или повтор флага.
- Установка в систему: `install.sh` (или `make install`/CMake) — бинарник в PATH, `.desktop` в меню приложений, SVG+PNG иконки камеры всех типоразмеров.

## Пакет
`onvif-camera-client-2.0.0-astra1.8-x86_64.tar.gz`:
```
bin/onvif-client          — приложение (Qt5 + libav)
install.sh                — установка в /usr/local (можно PREFIX=/opt ...)
share/applications/org.onvif.client.desktop
share/icons/hicolor/...   — иконки 16..256 + scalable SVG
README.txt
```
Требуются системные библиотеки Qt5 Widgets/Network и libav (штатные пакеты Astra Linux 1.8).
