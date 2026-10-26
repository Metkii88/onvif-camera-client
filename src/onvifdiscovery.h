#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QUuid>
#include <QHostAddress>
#include <QTimer>
#include <QStringList>

// Информация о камере, найденной по ONVIF (WS-Discovery)
struct CameraInfo
{
    QString address;      // IP-адрес камеры
    QString port;         // порт сервиса XAddr (если удалось разобрать)
    QString xaddr;        // полный URL сервиса, напр. http://192.168.1.10:8080/onvif/device_service
    QString manufacturer; // производитель из Scopes (опционально)
    QString model;        // модель из Scopes (опционально)
    QString types;        // сервисные типы (Device, Media и т.д.)
    QString rtspUrl;      // готовый RTSP-путь (по умолчанию /onvif1)

    QString displayString() const
    {
        QString s = address;
        if (!manufacturer.isEmpty()) {
            s += QString(" (%1").arg(manufacturer);
            if (!model.isEmpty())
                s += QString(" %1").arg(model);
            s += ")";
        } else if (!model.isEmpty()) {
            s += QString(" (%1)").arg(model);
        }
        return s;
    }
};

// Простой ONVIF-клиент: WS-Discovery (Probe multicast 239.255.255.250:3700)
class OnvifDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit OnvifDiscovery(QObject *parent = nullptr);

    // Запустить поиск длительностью durationMs мс
    void start(int durationMs = 5000);
    void stop();

signals:
    void cameraFound(const CameraInfo &info);
    void finished();

private slots:
    void onReadyRead();
    void onTimeout();

private:
    void sendProbe();

    QUdpSocket *m_socket = nullptr;
    QTimer *m_timer = nullptr;
    QStringList m_seen; // уже отправленные адреса, чтобы не дублировать
    QString m_matchId;
};
