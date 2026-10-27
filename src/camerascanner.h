#pragma once

#include <QDialog>
#include <QUdpSocket>
#include <QUuid>
#include <QTimer>
#include <QHostAddress>
#include <QList>
#include <QSet>
#include "cameradialog.h"

class QListWidget;
class QPushButton;
class QLabel;
class QSpinBox;
class QLineEdit;

// Результат сканирования одной точки
struct ScanResult
{
    QString ip;
    int port = 554;
    bool onvif = false;   // отозвалась на WS-Discovery
    bool rtsp  = false;   // открыт RTSP-порт
};

// Активный поиск камер: ONVIF multicast + TCP-сканирование подсети/диапазона
// с проверкой учётных данных шаблона (RTSP DESCRIBE).
class CameraScanner : public QDialog
{
    Q_OBJECT
public:
    explicit CameraScanner(const CameraTemplate &tpl, QWidget *parent = nullptr);

    QList<ScanResult> results() const { return m_results; }

private slots:
    void start();
    void stopAll();
    void onMulticastReady();
    void onProbeTimer();
    void onConnectChecked(uint64_t id, bool ok);

private:
    struct Pending
    {
        uint64_t id;
        QString ip;
        int port;
    };
    void scanNextBatch();
    void checkRtsp(const QString &ip, int port);
    void sendDiscoveryProbe();
    void addResult(const QString &ip, int port, bool onvif, bool rtsp);

    CameraTemplate m_tpl;

    QListWidget *m_list = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QLabel *m_status = nullptr;
    QSpinBox *m_fromSpin = nullptr;   // диапазон .N - .M
    QLineEdit *m_prefixEdit = nullptr; // 192.168.1

    QUdpSocket *m_udp = nullptr;
    QTimer *m_probeTimer = nullptr;
    QTimer *m_batchTimer = nullptr;
    QString m_matchId;

    QStringList m_queue;      // оставшиеся IP для проверки
    QList<Pending> m_pending; // соединения в процессе
    QList<ScanResult> m_results;
    QSet<QString> m_seen;
    int m_concurrency = 32;
    bool m_running = false;
};
