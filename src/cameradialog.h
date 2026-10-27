#pragma once

#include <QDialog>
#include "camerastore.h"

class QLineEdit;
class QLabel;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;

// Разобрать адрес из строки шаблона: "192.168.0.1-254", "192.168.0.1-30",
// "192.168.0.1", "192.168.0" (весь /24). Возвращает префикс ("192.168.0")
// и диапазон последних октетов; если строка не похожа на IPv4 — ok == false.
struct IpRangeParse
{
    bool ok = false;
    QString prefix;      // "192.168.0"
    int from = 1;
    int to = 254;
    bool isRange = false;
};

IpRangeParse parseIpRange(const QString &text);

// Шаблон поиска/подключения камеры: параметры сети + учётные данные.
// Используется и для ручного ввода RTSP, и для сохранения в хранилище.
struct CameraTemplate
{
    QString name;
    QString ip;                      // IP или диапазон: 192.168.0.1-254
    int     port = 554;
    QString path = QStringLiteral("/onvif1");
    QString user;
    QString password;
    QString codec = QStringLiteral("h264"); // подсказка пути потока при поиске
    bool    useRtspUrl = false;   // вместо ip/port/path — готовый RTSP URL
    QString rtspUrl;

    SavedCamera toSaved() const
    {
        SavedCamera c;
        c.name     = name;
        c.ip       = ip.trimmed();
        c.port     = port;
        c.path     = path.trimmed();
        c.user     = user;
        c.password = password;
        c.codec    = codec.trimmed().toLower();
        c.rtspPath = useRtspUrl ? rtspUrl.trimmed() : QString();
        return c;
    }

    static CameraTemplate fromSaved(const SavedCamera &c)
    {
        CameraTemplate t;
        t.name     = c.name;
        t.ip       = c.ip;
        t.port     = c.port;
        t.path     = c.path;
        t.user     = c.user;
        t.password = c.password;
        t.codec    = c.codec.isEmpty() ? QStringLiteral("h264") : c.codec;
        if (!c.rtspPath.isEmpty()) {
            t.useRtspUrl = true;
            t.rtspUrl    = c.rtspPath;
        }
        return t;
    }
};

// Диалог задания шаблона камеры: имя, IP (или диапазон), порт, путь, логин, пароль
class CameraDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CameraDialog(QWidget *parent = nullptr);

    void setTemplate(const CameraTemplate &t);   // для редактирования
    CameraTemplate templateValue() const;

private slots:
    void toggleMode(bool useRtsp);

private:
    QLineEdit   *m_name      = nullptr;
    QLineEdit   *m_ip        = nullptr;
    QSpinBox    *m_port      = nullptr;
    QLineEdit   *m_path      = nullptr;
    QComboBox   *m_codec     = nullptr;
    QLineEdit   *m_user      = nullptr;
    QLineEdit   *m_pass      = nullptr;
    QCheckBox   *m_rtspChk   = nullptr;
    QLineEdit   *m_rtspUrl   = nullptr;
    QPushButton *m_ok        = nullptr;
    QLabel      *m_rangeHint = nullptr;
};
