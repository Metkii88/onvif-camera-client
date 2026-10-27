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
class QComboBox;

// Результат сканирования одной точки
struct ScanResult
{
    QString ip;
    int port = 554;
    QString path;         // подобранный путь потока (пусто — не проверялся)
    QString rtspUrl;      // готовый URL для подключения/сохранения
    bool onvif = false;   // отозвалась на WS-Discovery
    bool rtsp  = false;   // RTSP DESCRIBE успешен с учётными данными шаблона
};

// Активный поиск камер: ONVIF multicast + TCP-сканирование подсети/диапазона
// с проверкой учётных данных шаблона (RTSP DESCRIBE) и подбором пути потока.
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
    void onConnectChecked(uint64_t id, bool ok, const QString &path);

private:
    struct Pending
    {
        uint64_t id;
        QString ip;
        int port;
    };
    void scanNextBatch();
    void checkRtsp(const QString &ip, int port);
    QStringList candidatePaths() const;
    void sendDiscoveryProbe();
    void addResult(const QString &ip, int port, bool onvif, bool rtsp,
                   const QString &path = QString());
    void rebuildQueueFromUi();

    CameraTemplate m_tpl;
    QStringList m_paths;          // перебираемые пути потоков

    QListWidget *m_list = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_prefixEdit = nullptr; // 192.168.0
    QSpinBox  *m_fromSpin = nullptr;
    QSpinBox  *m_toSpin = nullptr;
    QComboBox *m_codecCombo = nullptr;

    QUdpSocket *m_udp = nullptr;
    QTimer *m_probeTimer = nullptr;
    QTimer *m_batchTimer = nullptr;
    QTimer *m_progressTimer = nullptr;

    QStringList m_queue;      // оставшиеся IP для проверки
    QList<Pending> m_pending; // соединения в процессе
    QList<ScanResult> m_results;
    QSet<QString> m_seen;
    int m_concurrency = 32;
    int m_scanned = 0;        // сколько адресов уже проверено
    bool m_running = false;
};
