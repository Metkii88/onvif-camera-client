#pragma once

#include <QDialog>
#include "camerastore.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

// Шаблон поиска/подключения камеры: параметры сети + учётные данные.
// Используется и для ручного ввода RTSP, и для сохранения в хранилище.
struct CameraTemplate
{
    QString name;
    QString ip;
    int     port = 554;
    QString path = QStringLiteral("/onvif1");
    QString user;
    QString password;
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
    QLineEdit   *m_user      = nullptr;
    QLineEdit   *m_pass      = nullptr;
    QCheckBox   *m_rtspChk   = nullptr;
    QLineEdit   *m_rtspUrl   = nullptr;
    QPushButton *m_ok        = nullptr;
};
