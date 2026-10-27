#pragma once

#include <QString>
#include <QList>

// Сохранённая («предмет» набора) настроенная камера
struct SavedCamera
{
    QString name;      // отображаемое имя (например «Вход»)
    QString ip;        // IP-адрес / хост
    int port = 554;    // RTSP-порт
    QString path = QStringLiteral("/onvif1"); // путь потока
    QString user;      // логин
    QString password;  // пароль
    QString codec = QStringLiteral("h264"); // кодек/подсказка пути (h264, mjpeg...)
    QString rtspPath;  // готовый RTSP URL (если задан — перекрывает ip/port/path)

    QString url() const
    {
        if (!rtspPath.isEmpty())
            return rtspPath;
        return QStringLiteral("rtsp://%1:%2%3").arg(ip).arg(port).arg(path);
    }

    QString displayString() const
    {
        QString base = name.isEmpty() ? ip : name;
        return QStringLiteral("%1  [%2]").arg(base, url());
    }
};

// Хранилище сохранённых камер: JSON-файл в ~/.config/onvif-client/cameras.json
class CameraStore
{
public:
    static QString filePath();          // путь к файлу хранилища
    static QList<SavedCamera> load();   // прочитать список
    static bool save(const QList<SavedCamera> &list); // записать список
    static bool addOrUpdate(const SavedCamera &cam);  // добавить/обновить по имени+URL
    static bool removeAt(int index);    // удалить по индексу загруженного списка
};
