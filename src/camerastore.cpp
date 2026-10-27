#include "camerastore.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QString CameraStore::filePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + "/.config/onvif-client";
    QDir().mkpath(dir);
    return dir + "/cameras.json";
}

QList<SavedCamera> CameraStore::load()
{
    QList<SavedCamera> list;
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return list;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return list;
    const QJsonArray arr = doc.object().value("cameras").toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        SavedCamera c;
        c.name     = o.value("name").toString();
        c.ip       = o.value("ip").toString();
        c.port     = o.value("port").toInt(554);
        c.path     = o.value("path").toString("/onvif1");
        c.user     = o.value("user").toString();
        c.password = o.value("password").toString();
        c.rtspPath = o.value("rtsp").toString();
        if (!c.ip.isEmpty() || !c.rtspPath.isEmpty())
            list.append(c);
    }
    return list;
}

bool CameraStore::save(const QList<SavedCamera> &list)
{
    QJsonArray arr;
    for (const SavedCamera &c : list) {
        QJsonObject o;
        o["name"]     = c.name;
        o["ip"]       = c.ip;
        o["port"]     = c.port;
        o["path"]     = c.path;
        o["user"]     = c.user;
        o["password"] = c.password;
        o["rtsp"]     = c.rtspPath;
        arr.append(o);
    }
    QJsonObject root;
    root["cameras"] = arr;

    QFile f(filePath());
    // файл с паролями — только для владельца
    if (f.exists())
        f.remove();
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    QFile::setPermissions(filePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool CameraStore::addOrUpdate(const SavedCamera &cam)
{
    QList<SavedCamera> list = load();
    for (auto &c : list) {
        if (c.url() == cam.url() && (c.name == cam.name || cam.name == c.name)) {
            c = cam;
            return save(list);
        }
    }
    // если имя занято другим URL — обновим тот же элемент по имени
    for (auto &c : list) {
        if (!cam.name.isEmpty() && c.name == cam.name) {
            c = cam;
            return save(list);
        }
    }
    list.append(cam);
    return save(list);
}

bool CameraStore::removeAt(int index)
{
    QList<SavedCamera> list = load();
    if (index < 0 || index >= list.size())
        return false;
    list.removeAt(index);
    return save(list);
}
