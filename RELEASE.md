# Release v2.1.0 — новый дизайн

## Изменения относительно v2.0
- Тёмная тема оформления (QSS): читаемые кнопки, поля, списки, меню, скроллбары
- Шапка приложения с названием и версией
- Опция `--theme <file.qss>` и внешний файл `themes/style.qss` (правка без пересборки)
- Обновлённый установщик: тема в `/usr/share/onvif-camera-client/themes`, рабочий menu-launcher

## Аттач
`onvif-camera-client-2.1.0-astra1.8-x86_64.tar.gz` — самодостаточный пакет (~82 МБ):
бинарник, Qt5+FFmpeg рантайм, плагины, тема, иконки, desktop-файл, run.sh, install.sh.

MD5: 623b22fa5a50d65177e00bcd3777cb4c

## Установка
```bash
tar xzf onvif-camera-client-2.1.0-astra1.8-x86_64.tar.gz
cd onvif-camera-client-2.1.0 && ./run.sh          # или sudo ./install.sh
```
