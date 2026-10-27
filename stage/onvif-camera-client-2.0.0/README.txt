ONVIF Camera Client v2.0.0 — Astra Linux 1.8 (x86_64)

Зависимости (из репозитория Astra):
  apt install libqt5widgets5 libqt5network5 libavformat57 libavcodec57 libavutil55 libswscale4
  (или более новые версии libav*, доступные в вашей конфигурации)

Установка в систему (иконка в меню + bin в PATH):
  sudo ./install.sh

Запуск:
  onvif-client                          — обычный режим
  onvif-client --fullscreen             — полноэкранный режим
  onvif-client -p "Двор,Вход" -f        — полный экран с подключением сохранённых камер

Горячие клавиши: F11 — полноэкранный режим, Esc — выход из полного экрана.

Сохранённые камеры хранятся в ~/.config/OnvifClient/cameras.json
(пароли обфусцированы base64; файл не предназначен для защиты от локального атакующего).

Поиск: WS-Discovery (ONVIF) по сети + ручной шаблон: адрес/порт, логин, пароль, путь RTSP.
