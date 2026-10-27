#!/bin/bash
# Установка клиента ONVIF в систему (Astra Linux 1.8). Запускать от root: sudo ./install.sh
set -e
DIR=$(dirname "$(readlink -f "$0")")
PREFIX=${PREFIX:-/usr/local}
install -m 755 -d "$PREFIX/bin" "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor/scalable/apps"
install -m 755 "$DIR/bin/onvif-client" "$PREFIX/bin/onvif-client"
install -m 644 "$DIR/share/applications/org.onvif.client.desktop" "$PREFIX/share/applications/"
install -m 644 "$DIR/share/icons/hicolor/scalable/apps/org.onvif.client.svg" "$PREFIX/share/icons/hicolor/scalable/apps/"
for f in $(find "$DIR/share/icons/hicolor" -name '*.png'); do
    sz=$(basename "$(dirname "$(dirname "$f")")")
    install -m 755 -d "$PREFIX/share/icons/hicolor/$sz/apps"
    install -m 644 "$f" "$PREFIX/share/icons/hicolor/$sz/apps/"
done
command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor" || true
echo "Установлено в $PREFIX. Запуск: onvif-client или из меню ('Камеры ONVIF')."
echo "Полноэкранный запуск с профилем: onvif-client --fullscreen --profile \"Камера1,Камера2\""
