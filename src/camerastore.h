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

// ---------- Профили сетки ----------
// Профиль = раскладка сетки (колонки) + список подключённых камер (по имени
// из хранилища либо полным RTSP URL, если камера не сохранена).

// Одна запись профиля: привязка камеры к позиции в сетке
struct ProfileEntry
{
    int cellIndex = -1;     // позиция ячейки в сетке (0..N)
    QString camName;        // имя сохранённой камеры (если есть)
    QString url;            // RTSP URL (резерв / для несохранённых камер)
    QString user;
    QString password;
    QString title;          // отображаемое имя в ячейке
};

struct CameraProfile
{
    QString name;
    int cols = 2;
    QList<ProfileEntry> entries;
};

// Хранилище профилей: ~/.config/onvif-client/profiles.json
class ProfileStore
{
public:
    static QString filePath();
    static QList<CameraProfile> load();
    static bool saveAll(const QList<CameraProfile> &list);
    // добавить/заменить профиль по имени
    static bool addOrUpdate(const CameraProfile &p);
    static bool removeByName(const QString &name);
    static bool rename(const QString &oldName, const QString &newName);
};
