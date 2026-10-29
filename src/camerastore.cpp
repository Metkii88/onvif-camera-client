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
        c.codec    = o.value("codec").toString(QStringLiteral("h264"));
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
        o["codec"]    = c.codec;
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

// ============================ Профили сетки ============================

QString ProfileStore::filePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + "/.config/onvif-client";
    QDir().mkpath(dir);
    return dir + "/profiles.json";
}

QList<CameraProfile> ProfileStore::load()
{
    QList<CameraProfile> list;
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return list;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return list;
    const QJsonArray arr = doc.object().value("profiles").toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject po = v.toObject();
        CameraProfile p;
        p.name = po.value("name").toString();
        p.cols = po.value("cols").toInt(2);
        const QJsonArray cams = po.value("cameras").toArray();
        for (const QJsonValue &cv : cams) {
            const QJsonObject co = cv.toObject();
            ProfileEntry e;
            e.cellIndex = co.value("cell").toInt(-1);
            e.camName   = co.value("name").toString();
            e.url       = co.value("url").toString();
            e.user      = co.value("user").toString();
            e.password  = co.value("password").toString();
            e.title     = co.value("title").toString();
            p.entries.append(e);
        }
        if (!p.name.isEmpty())
            list.append(p);
    }
    return list;
}

bool ProfileStore::saveAll(const QList<CameraProfile> &list)
{
    QJsonArray arr;
    for (const CameraProfile &p : list) {
        QJsonObject po;
        po["name"] = p.name;
        po["cols"] = p.cols;
        QJsonArray cams;
        for (const ProfileEntry &e : p.entries) {
            QJsonObject co;
            co["cell"]     = e.cellIndex;
            co["name"]     = e.camName;
            co["url"]      = e.url;
            co["user"]     = e.user;
            co["password"] = e.password;
            co["title"]    = e.title;
            cams.append(co);
        }
        po["cameras"] = cams;
        arr.append(po);
    }
    QJsonObject root;
    root["profiles"] = arr;

    const QString path = filePath();
    const QString tmp  = path + ".tmp";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    QFile::remove(path);
    return QFile::rename(tmp, path);
}

bool ProfileStore::addOrUpdate(const CameraProfile &p)
{
    QList<CameraProfile> list = load();
    for (CameraProfile &ex : list) {
        if (ex.name.compare(p.name, Qt::CaseInsensitive) == 0) {
            ex = p;
            return saveAll(list);
        }
    }
    list.append(p);
    return saveAll(list);
}

bool ProfileStore::removeByName(const QString &name)
{
    QList<CameraProfile> list = load();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            list.removeAt(i);
            return saveAll(list);
        }
    }
    return false;
}

bool ProfileStore::rename(const QString &oldName, const QString &newName)
{
    if (newName.trimmed().isEmpty() || oldName == newName.trimmed())
        return false;
    QList<CameraProfile> list = load();
    for (CameraProfile &p : list) {
        if (p.name.compare(oldName, Qt::CaseInsensitive) == 0) {
            p.name = newName.trimmed();
            return saveAll(list);
        }
    }
    return false;
}
