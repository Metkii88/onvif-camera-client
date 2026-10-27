#include "camerascanner.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QCoreApplication>

// --- упрощённая проверка RTSP DESCRIBE с Basic-авторизацией -----------------
class RtspCheck : public QObject
{
    Q_OBJECT
public:
    RtspCheck(const QString &ip, int port, const QString &user, const QString &pass,
              uint64_t id, QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_ip(ip), m_port(port), m_user(user), m_pass(pass)
    {
        m_sock = new QTcpSocket(this);
        m_sock->connectToHost(ip, quint16(port));
        connect(m_sock, &QTcpSocket::connected, this, &RtspCheck::onConnected);
        connect(m_sock, &QTcpSocket::readyRead,  this, &RtspCheck::onReady);
        connect(m_sock, &QAbstractSocket::errorOccurred,
                this, [this] { emit done(m_id, false); });
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, [this] { cleanup(); emit done(m_id, false); });
        m_timer->start(3000);
    }

signals:
    void done(uint64_t id, bool rtspOk);

private slots:
    void onConnected()
    {
        // соединение с портом установлено — порт открыт; пробуем DESCRIBE
        sendDescribe(QByteArray());
    }
    void onReady()
    {
        m_resp += QString::fromUtf8(m_sock->readAll());
        if (m_resp.contains("RTSP/1.0 200")) { cleanup(); emit done(m_id, true); return; }
        if (m_resp.contains("RTSP/1.0 401") && !m_authSent) {
            QByteArray cred = (m_user + ":" + m_pass).toUtf8().toBase64();
            sendDescribe(cred.constData());
            return;
        }
        if (m_resp.contains("RTSP/1.0")) { cleanup(); emit done(m_id, false); }
    }

private:
    void sendDescribe(const QByteArray &authB64)
    {
        m_authSent = true;
        QString req = QString("DESCRIBE %1 RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: onvif-client\r\n")
                          .arg(QStringLiteral("rtsp://%1/onvif1").arg(m_ip));
        if (!authB64.isEmpty())
            req += QString("Authorization: Basic %1\r\n").arg(QString::fromLatin1(authB64));
        req += "Accept: */*\r\n\r\n";
        m_sock->write(req.toUtf8());
    }
    void cleanup()
    {
        m_timer->stop();
        m_sock->abort();
        deleteLater();
    }

    uint64_t m_id;
    QString m_ip;
    int m_port;
    QString m_user, m_pass;
    QTcpSocket *m_sock = nullptr;
    QTimer *m_timer = nullptr;
    QString m_resp;
    bool m_authSent = false;
};

// -----------------------------------------------------------------------------

static const char *kProbeTpl = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
            xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing"
            xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
            xmlns:dn="http://www.onvif.org/ver10/network/wsdl">
  <s:Header>
    <a:MessageID>urn:uuid:%1</a:MessageID>
    <a:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To>
    <a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action>
  </s:Header>
  <s:Body>
    <d:Probe><d:Types>dn:NetworkVideoTransmitter</d:Types></d:Probe>
  </s:Body>
</s:Envelope>)";

CameraScanner::CameraScanner(const CameraTemplate &tpl, QWidget *parent)
    : QDialog(parent), m_tpl(tpl)
{
    setWindowTitle(QStringLiteral("Поиск камер по шаблону"));
    resize(560, 420);

    m_prefixEdit = new QLineEdit;
    // префикс из шаблона: 192.168.1.x -> 192.168.1
    QString ipPart = tpl.useRtspUrl ? QString() : tpl.ip;
    QStringList secs = ipPart.split('.');
    if (secs.size() == 4 && (secs[3].contains('-') || secs[3].toInt() > 0))
        m_prefixEdit->setText(secs[0] + "." + secs[1] + "." + secs[2]);
    else
        m_prefixEdit->setText(QStringLiteral("192.168.1"));

    auto *fromL = new QLabel(QStringLiteral("Диапазон хостов:"));
    m_fromSpin = new QSpinBox;
    m_fromSpin->setRange(1, 254);
    m_fromSpin->setValue(1);
    auto *dash = new QLabel("-");
    auto m_toSpin = new QSpinBox;
    m_toSpin->setRange(1, 254);
    m_toSpin->setValue(254);
    m_toSpin->setObjectName("toSpin");

    m_startBtn = new QPushButton(QStringLiteral("🔍 Начать поиск"));
    m_stopBtn  = new QPushButton(QStringLiteral("Стоп"));
    m_stopBtn->setEnabled(false);
    m_status   = new QLabel(QStringLiteral("ONVIF-рассылка + проверка RTSP порта %1.\nЛогин: %2")
                                .arg(tpl.port)
                                .arg(tpl.user.isEmpty() ? QStringLiteral("без авторизации") : tpl.user));

    connect(m_startBtn, &QPushButton::clicked, this, &CameraScanner::start);
    connect(m_stopBtn,  &QPushButton::clicked, this, &CameraScanner::stopAll);

    m_list = new QListWidget;
    m_list->setAlternatingRowColors(true);

    auto *rangeRow = new QHBoxLayout;
    rangeRow->addWidget(fromL);
    rangeRow->addWidget(m_fromSpin);
    rangeRow->addWidget(dash);
    rangeRow->addWidget(m_toSpin);
    rangeRow->addStretch();

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();

    auto *subnetRow = new QHBoxLayout;
    subnetRow->addWidget(new QLabel(QStringLiteral("Подсеть:")));
    subnetRow->addWidget(m_prefixEdit);
    subnetRow->addStretch();

    auto *root = new QVBoxLayout(this);
    root->addLayout(subnetRow);
    root->addLayout(rangeRow);
    root->addLayout(btnRow);
    root->addWidget(m_list, 1);
    root->addWidget(m_status);

    auto *saveBtn = new QPushButton(QStringLiteral("💾 Сохранить найденные в хранилище"));
    auto *closeBtn = new QPushButton(QStringLiteral("Закрыть"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(saveBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(closeBtn);
    root->addLayout(bottomRow);

    connect(saveBtn, &QPushButton::clicked, this, [this, saveBtn] {
        int saved = 0;
        for (const ScanResult &r : m_results) {
            SavedCamera c;
            c.name = r.ip;
            c.ip   = r.ip;
            c.port = r.port;
            c.path = m_tpl.path;
            c.user = m_tpl.user;
            c.password = m_tpl.password;
            if (CameraStore::addOrUpdate(c)) ++saved;
        }
        saveBtn->setText(QStringLiteral("Сохранено: %1 ✓").arg(saved));
    });

    // UDP для multicast-проб
    m_udp = new QUdpSocket(this);
    m_udp->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_udp->joinMulticastGroup(QHostAddress("239.255.255.250"));
    connect(m_udp, &QUdpSocket::readyRead, this, &CameraScanner::onMulticastReady);

    m_probeTimer = new QTimer(this);
    connect(m_probeTimer, &QTimer::timeout, this, &CameraScanner::onProbeTimer);

    m_batchTimer = new QTimer(this);
    m_batchTimer->setInterval(120);
    connect(m_batchTimer, &QTimer::timeout, this, &CameraScanner::scanNextBatch);
}

void CameraScanner::start()
{
    if (m_running)
        return;
    m_running = true;
    m_results.clear();
    m_seen.clear();
    m_list->clear();
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    // если в шаблоне указан конкретный IP (не диапазон) — проверяем только его
    QString lastOctet = m_tpl.ip.section('.', 3, 3);
    m_queue.clear();
    if (!m_tpl.useRtspUrl && lastOctet.contains('-')) {
        const QStringList parts = lastOctet.split('-');
        bool ok1 = false, ok2 = false;
        int a = parts[0].toInt(&ok1), b = parts[1].toInt(&ok2);
        if (ok1 && ok2) {
            const QString pfx = m_tpl.ip.section('.', 0, 2);
            for (int i = qMin(a,b); i <= qMax(a,b); ++i)
                m_queue << QStringLiteral("%1.%2").arg(pfx).arg(i);
        }
    }
    if (m_queue.isEmpty()) {
        const QString pfx = m_prefixEdit->text().trimmed();
        auto *toSpin = findChild<QSpinBox*>("toSpin");
        const int from = m_fromSpin->value(), to = toSpin ? toSpin->value() : 254;
        if (pfx.count('.') == 2)
            for (int i = from; i <= to; ++i)
                m_queue << QStringLiteral("%1.%2").arg(pfx).arg(i);
    }

    m_probeTimer->start(1500);   // переотправка Probe каждые 1.5 с
    sendDiscoveryProbe();
    m_batchTimer->start();
    m_status->setText(QStringLiteral("Сканирую %1 адресов...")
                      .arg(m_queue.size() + 254));
}

void CameraScanner::stopAll()
{
    m_running = false;
    m_probeTimer->stop();
    m_batchTimer->stop();
    m_queue.clear();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_status->setText(QStringLiteral("Найдено: %1").arg(m_results.size()));
}

void CameraScanner::onProbeTimer()
{
    sendDiscoveryProbe();
}

void CameraScanner::sendDiscoveryProbe()
{
    const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QByteArray body =
        QString::fromUtf8(kProbeTpl).arg(msgId).toUtf8();
    for (const QNetworkInterface &ni : QNetworkInterface::allInterfaces()) {
        if (!(ni.flags() & QNetworkInterface::CanMulticast))
            continue;
        m_udp->writeDatagram(body, QHostAddress("239.255.255.250"), 3700);
    }
}

void CameraScanner::onMulticastReady()
{
    while (m_udp->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_udp->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_udp->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        const QString text = QString::fromUtf8(datagram);
        if (!text.contains("ProbeMatches"))
            continue;
        // извлечь XAddr
        int idx = text.indexOf("<d:XAddr>");
        QString xaddr;
        if (idx >= 0) {
            int end = text.indexOf("</d:XAddr>", idx);
            xaddr = text.mid(idx + 9, end - idx - 9);
        }
        QString ip = sender.toString();
        if (xaddr.contains("://")) {
            QString h = xaddr.section("://", 1).section('/', 0, 0).section(':', 0, 0);
            if (!h.isEmpty()) ip = h;
        }
        addResult(ip, m_tpl.port, true, false);
    }
}

void CameraScanner::scanNextBatch()
{
    if (!m_running)
        return;
    if (m_queue.isEmpty()) {
        if (m_pending.isEmpty()) {
            stopAll();
            m_status->setText(QStringLiteral("Поиск завершён. Найдено: %1")
                                  .arg(m_results.size()));
        }
        return;
    }
    while (!m_queue.isEmpty() && m_pending.size() < m_concurrency) {
        const QString ip = m_queue.takeFirst();
        checkRtsp(ip, m_tpl.port);
    }
}

void CameraScanner::checkRtsp(const QString &ip, int port)
{
    static uint64_t s_id = 0;
    const uint64_t id = ++s_id;
    Pending p{id, ip, port};
    m_pending.append(p);

    auto *chk = new RtspCheck(ip, port, m_tpl.user, m_tpl.password, id, this);
    connect(chk, &RtspCheck::done, this, &CameraScanner::onConnectChecked);
}

void CameraScanner::onConnectChecked(uint64_t id, bool ok)
{
    for (int i = 0; i < m_pending.size(); ++i) {
        if (m_pending[i].id == id) {
            const Pending p = m_pending[i];
            m_pending.removeAt(i);
            if (ok)
                addResult(p.ip, p.port, false, true);
            break;
        }
    }
}

void CameraScanner::addResult(const QString &ip, int port, bool onvif, bool rtsp)
{
    const QString key = QStringLiteral("%1:%2").arg(ip).arg(port);
    if (m_seen.contains(key)) {
        // обновить флаг ONVIF, если ранее нашли только по порту
        for (auto &r : m_results)
            if (r.ip == ip && r.port == port && onvif)
                r.onvif = true;
        return;
    }
    m_seen.insert(key);
    ScanResult res{ip, port, onvif, rtsp};
    m_results.append(res);

    QString label = QStringLiteral("%1:%2").arg(ip).arg(port);
    if (onvif) label += QStringLiteral("  [ONVIF]");
    if (rtsp)  label += QStringLiteral("  [RTSP ✓]");
    m_list->addItem(label);
    m_status->setText(QStringLiteral("Найдено: %1").arg(m_results.size()));
}

#include "camerascanner.moc"
