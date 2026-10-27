#include "camerascanner.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QUuid>

// --- упрощённая проверка RTSP DESCRIBE с Basic/Digest-авторизацией -----------
// Ключевые исправления v2.2:
//  * авторизация (Basic) отправляется СРАЗУ при первом DESCRIBE — многие камеры
//    (Hikvision/Dahua) на анонимный DESCRITE отвечают 401 и закрывают соединение,
//    из-за чего раньше поиск «не находил» камеры даже с верными admin/admin;
//  * ответы буферизуются побайтово (raw bytes), чтобы не терять данные при
//    разбиении TCP-пакетов; разбор статуса — по первой строке ответа;
//  * корректная обработка Digest-ответа (realm/nonce/qop) в дополнение к Basic;
//  * при 401 на текущем пути пробуем следующий путь (с авторизацией), а не
//    зацикливаемся; таймаут общий на все пути.
class RtspCheck : public QObject
{
    Q_OBJECT
public:
    RtspCheck(const QString &ip, int port, const QString &user, const QString &pass,
              const QStringList &paths, uint64_t id, QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_ip(ip), m_port(port),
          m_user(user), m_pass(pass), m_paths(paths)
    {
        m_sock = new QTcpSocket(this);
        m_sock->connectToHost(ip, quint16(port));
        connect(m_sock, &QTcpSocket::connected, this, &RtspCheck::onConnected);
        connect(m_sock, &QTcpSocket::readyRead,  this, &RtspCheck::onReady);
        connect(m_sock, &QAbstractSocket::disconnected, this, [this] {
            if (!m_finished)
                nextPath();     // сервер мог закрыть сокет после 401 — пробуем другой путь
        });
        connect(m_sock, &QAbstractSocket::errorOccurred, this, [this] {
            finish(false, QString());   // порт закрыт/недоступен
        });
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, [this] {
            finish(false, QString());
        });
        m_timer->start(9000);   // общий бюджет на перебор путей
    }

signals:
    void done(uint64_t id, bool rtspOk, const QString &path);

private slots:
    void onConnected()
    {
        // соединение установлено — пробуем DESCRIBE по первому пути
        sendDescribe();
    }
    void onReady()
    {
        m_raw.append(m_sock->readAll());
        processBuffer();
    }

private:
    void processBuffer()
    {
        const QString text = QString::fromLatin1(m_raw);
        // первая строка ответа: RTSP/1.0 <код> <текст>
        static const QRegularExpression reStatus("RTSP/1\\.0\\s+(\\d{3})");
        const QRegularExpressionMatch ms = reStatus.match(text);
        if (!ms.hasMatch())
            return;             // статус ещё не пришёл — ждём данных
        const int code = ms.captured(1).toInt();

        if (code == 200) {
            // нужен заголовок Content-Type или тело SDP — дождёмся окончания заголовков
            if (!text.contains("\r\n\r\n"))
                return;         // заголовки ещё не полностью получены
            finish(true, m_paths.value(m_pathIdx));
            return;
        }
        if (code == 401) {
            if (!text.contains("\r\n\r\n"))
                return;         // дождёмся WWW-Authenticate
            handleAuthChallenge(text);
            return;
        }
        // 404 / 400 / 5xx — путь не подходит, пробуем следующий
        nextPath();
    }

    void handleAuthChallenge(const QString &text)
    {
        const int idx = text.indexOf("WWW-Authenticate:", 0, Qt::CaseInsensitive);
        const QString challenge = (idx >= 0)
            ? text.mid(idx + 17, text.indexOf("\r\n", idx) - idx - 17).trimmed()
            : QString();

        if (challenge.startsWith("Digest", Qt::CaseInsensitive)) {
            m_digestRealm = extractParam(challenge, "realm");
            m_digestNonce = extractParam(challenge, "nonce");
            m_digestQop   = extractParam(challenge, "qop");
            m_authMode    = Digest;
        } else {
            m_authMode    = BasicRequired;
        }

        if (m_user.isEmpty()) {
            // учётных данных нет, а сервер их требует — путь недоступен
            nextPath();
            return;
        }
        if (m_authTriedOnThisPath) {
            // уже пробовали авторизоваться на этом пути — сервер отклоняет учётки/путь
            nextPath();
            return;
        }
        m_authTriedOnThisPath = true;
        sendDescribe();
    }

    static QString extractParam(const QString &challenge, const QString &name)
    {
        const QRegularExpression re(QStringLiteral("%1=(?:\\\"([^\\\"]*)\\\"|([^,\\s]+))")
                                    .arg(QRegularExpression::escape(name)),
                                    QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(challenge);
        if (!m.hasMatch())
            return QString();
        return m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
    }

    void nextPath()
    {
        if (m_finished)
            return;
        if (m_pathIdx + 1 < m_paths.size()) {
            ++m_pathIdx;
            m_authTriedOnThisPath = false;
            // если соединение живо — пробуем следующий путь по тому же сокету;
            // иначе переподключаемся
            if (m_sock->state() == QAbstractSocket::ConnectedState)
                sendDescribe();
            else
                m_sock->connectToHost(m_ip, quint16(m_port));
        } else {
            finish(false, QString());
        }
    }

    void sendDescribe()
    {
        if (m_finished)
            return;
        const QString path = m_paths.value(m_pathIdx);
        const QString uri = QStringLiteral("rtsp://%1:%2%3").arg(m_ip).arg(m_port).arg(path);
        QString req = QString("DESCRIBE %1 RTSP/1.0\r\n"
                              "CSeq: %2\r\n"
                              "User-Agent: onvif-client\r\n"
                              "Accept: application/sdp\r\n")
                          .arg(uri).arg(++m_cseq);

        // Many cameras answer 200 immediately; but Hikvision/Dahua answer 401 and
        // close the socket — so we always send Basic credentials up-front when set.
        if (!m_user.isEmpty()) {
            if (m_authMode == Digest && !m_digestNonce.isEmpty()) {
                const QByteArray ha1 = QCryptographicHash::hash(
                    (m_user + ":" + m_digestRealm + ":" + m_pass).toUtf8(),
                    QCryptographicHash::Md5);
                const QByteArray ha2 = QCryptographicHash::hash(
                    (QStringLiteral("DESCRIBE:") + uri).toUtf8(),
                    QCryptographicHash::Md5);
                const QString cnonce = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
                const QString nc = "00000001";
                QByteArray respInput;
                QString extra;
                if (!m_digestQop.isEmpty()) {
                    respInput = ha1 + ":" + m_digestNonce.toUtf8() + ":" + nc.toUtf8() + ":"
                              + cnonce.toUtf8() + ":auth:" + ha2;
                    extra = QString(",qop=auth,nc=%1,cnonce=\"%2\"").arg(nc, cnonce);
                } else {
                    respInput = ha1 + ":" + m_digestNonce.toUtf8() + ":" + ha2;
                }
                const QString response = QString::fromLatin1(
                    QCryptographicHash::hash(respInput, QCryptographicHash::Md5).toHex());
                req += QString("Authorization: Digest username=\"%1\",realm=\"%2\","
                               "nonce=\"%3\",uri=\"%4\",response=\"%5\"%6\r\n")
                           .arg(m_user, m_digestRealm, m_digestNonce, uri, response, extra);
            } else {
                const QByteArray cred = (m_user + ":" + m_pass).toUtf8().toBase64();
                req += QString("Authorization: Basic %1\r\n").arg(QString::fromLatin1(cred));
            }
        }
        req += "\r\n";
        m_raw.clear();
        m_sock->write(req.toUtf8());
        m_sock->flush();
    }

    void finish(bool ok, const QString &path)
    {
        if (m_finished)
            return;
        m_finished = true;
        m_timer->stop();
        m_sock->disconnect(this);
        m_sock->abort();
        emit done(m_id, ok, path);
        deleteLater();
    }

    uint64_t m_id;
    QString m_ip;
    int m_port;
    QString m_user, m_pass;
    QStringList m_paths;
    int m_pathIdx = 0;
    int m_cseq = 0;
    QTcpSocket *m_sock = nullptr;
    QTimer *m_timer = nullptr;
    QByteArray m_raw;
    enum AuthMode { None, BasicRequired, Digest } m_authMode = None;
    bool m_authTriedOnThisPath = false;
    bool m_finished = false;
    QString m_digestRealm, m_digestNonce, m_digestQop;
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
    resize(640, 460);

    // --- префикс подсети из шаблона (поддерживается диапазон x.x.x.1-254) ---
    m_prefixEdit = new QLineEdit;
    const IpRangeParse pr = parseIpRange(tpl.ip);
    if (pr.ok)
        m_prefixEdit->setText(pr.prefix);
    else if (!tpl.useRtspUrl && !tpl.ip.isEmpty())
        m_prefixEdit->setText(tpl.ip.section('.', 0, 2));
    else
        m_prefixEdit->setText(QStringLiteral("192.168.0"));

    auto *fromL = new QLabel(QStringLiteral("Диапазон хостов:"));
    m_fromSpin = new QSpinBox;
    m_fromSpin->setRange(1, 254);
    m_fromSpin->setValue(pr.ok ? pr.from : 1);
    auto *dash = new QLabel("-");
    m_toSpin = new QSpinBox;
    m_toSpin->setRange(1, 254);
    m_toSpin->setValue(pr.ok ? pr.to : 254);

    m_codecCombo = new QComboBox;
    m_codecCombo->addItems({QStringLiteral("h264"), QStringLiteral("mjpeg"),
                            QStringLiteral("h265"), QStringLiteral("any")});
    m_codecCombo->setEditable(true);
    m_codecCombo->setCurrentText(tpl.codec.isEmpty()
                                    ? QStringLiteral("h264") : tpl.codec);

    m_startBtn = new QPushButton(QStringLiteral("🔍 Начать поиск"));
    m_startBtn->setObjectName("btnPrimary");
    m_stopBtn  = new QPushButton(QStringLiteral("Стоп"));
    m_stopBtn->setEnabled(false);
    m_status   = new QLabel(QStringLiteral(
                "ONVIF-рассылка + проверка RTSP порта %1 (логин «%2»),\n"
                "при необходимости подбирается путь потока.")
                .arg(tpl.port)
                .arg(tpl.user.isEmpty() ? QStringLiteral("без авторизации") : tpl.user));
    m_status->setWordWrap(true);

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

    auto *optsForm = new QFormLayout;
    auto *subnetRow = new QHBoxLayout;
    subnetRow->addWidget(m_prefixEdit);
    subnetRow->addWidget(new QLabel(QStringLiteral(".x")));
    subnetRow->addStretch();
    optsForm->addRow(QStringLiteral("Подсеть:"), subnetRow);
    auto *codecRow = new QHBoxLayout;
    codecRow->addWidget(m_codecCombo);
    codecRow->addStretch();
    optsForm->addRow(QStringLiteral("Кодек:"), codecRow);

    auto *root = new QVBoxLayout(this);
    root->addLayout(optsForm);
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
            c.path = r.path.isEmpty() ? m_tpl.path : r.path;
            c.user = m_tpl.user;
            c.password = m_tpl.password;
            c.codec = m_codecCombo->currentText().trimmed().toLower();
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
    m_batchTimer->setInterval(60);
    connect(m_batchTimer, &QTimer::timeout, this, &CameraScanner::scanNextBatch);

    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(500);
    connect(m_progressTimer, &QTimer::timeout, this, [this] {
        if (!m_running)
            return;
        const int total = m_scanned + m_queue.size();
        m_status->setText(QStringLiteral("Сканирование: %1/%2 адресов… Найдено: %3")
                              .arg(m_scanned).arg(total).arg(m_results.size()));
    });
}

QStringList CameraScanner::candidatePaths() const
{
    QStringList paths;
    auto add = [&paths](const QString &p) {
        const QString norm = p.startsWith('/') ? p : '/' + p;
        if (!paths.contains(norm))
            paths << norm;
    };

    // 1) путь из шаблона — всегда первым (пользователь задал его явно)
    const QString tplPath = m_tpl.path.trimmed();
    if (!tplPath.isEmpty())
        add(tplPath);

    // 2) типовые ONVIF-пути
    add("/onvif");
    add("/onvif1");      // Hikvision
    add("/onvif2");
    add("/media/video1");

    const QString codec = m_codecCombo->currentText().trimmed().toLower();
    if (codec == "mjpeg") {
        add("/video/mjpeg");
        add("/mjpeg/feed?Rate=10&Qual=50");
        add("/videocap.jpg");
    } else {
        add("/Streaming/Channels/101");   // Hikvision main
        add("/Streaming/Channels/102");   // Hikvision sub
        add("/cam/realmonitor?channel=1&subtype=0"); // Dahua main
        add("/cam/realmonitor?channel=1&subtype=1"); // Dahua sub
        add("/unicast");                  // Axis
        add("/live/ch00_0");              // Some Chinese OEM
        add("/mpeg4/channel1/media.amp"); // older Hikvision
        add("/h264");                      // generic
        add("/av0_0");
    }
    return paths;
}

void CameraScanner::rebuildQueueFromUi()
{
    m_queue.clear();
    const QString pfx = m_prefixEdit->text().trimmed();
    if (pfx.count('.') != 2)
        return;
    const int from = m_fromSpin->value();
    const int to   = m_toSpin->value();
    for (int i = qMin(from, to); i <= qMax(from, to); ++i)
        m_queue << QStringLiteral("%1.%2").arg(pfx).arg(i);
}

void CameraScanner::start()
{
    if (m_running)
        return;
    m_running = true;
    m_results.clear();
    m_seen.clear();
    m_scanned = 0;
    m_list->clear();
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_paths = candidatePaths();

    rebuildQueueFromUi();
    if (m_queue.isEmpty()) {
        m_running = false;
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_status->setText(QStringLiteral("Укажите подсеть вида 192.168.0 и диапазон хостов."));
        return;
    }

    m_probeTimer->start(1500);   // переотправка Probe каждые 1.5 с
    sendDiscoveryProbe();
    m_batchTimer->start();
    m_progressTimer->start();
    m_status->setText(QStringLiteral("Сканирую %1 адресов (путей: %2)...")
                      .arg(m_queue.size()).arg(m_paths.size()));
}

void CameraScanner::stopAll()
{
    m_running = false;
    m_probeTimer->stop();
    m_batchTimer->stop();
    m_progressTimer->stop();
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
        // IP вне сканируемой подсети тоже интересен — добавляем как ONVIF-находку
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
            m_status->setText(QStringLiteral("Поиск завершён. Проверено адресов: %1. Найдено: %2")
                                  .arg(m_scanned).arg(m_results.size()));
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

    auto *chk = new RtspCheck(ip, port, m_tpl.user, m_tpl.password, m_paths, id, this);
    connect(chk, &RtspCheck::done, this, &CameraScanner::onConnectChecked);
}

void CameraScanner::onConnectChecked(uint64_t id, bool ok, const QString &path)
{
    ++m_scanned;
    for (int i = 0; i < m_pending.size(); ++i) {
        if (m_pending[i].id == id) {
            const Pending p = m_pending[i];
            m_pending.removeAt(i);
            if (ok)
                addResult(p.ip, p.port, false, true, path);
            break;
        }
    }
}

void CameraScanner::addResult(const QString &ip, int port, bool onvif, bool rtsp,
                              const QString &path)
{
    const QString key = QStringLiteral("%1:%2").arg(ip).arg(port);
    if (m_seen.contains(key)) {
        // обновить запись: RTSP подтверждён / появился путь / флаг ONVIF
        for (auto &r : m_results) {
            if (r.ip == ip && r.port == port) {
                bool changed = false;
                if (onvif && !r.onvif) { r.onvif = true; changed = true; }
                if (rtsp && !r.rtsp)   { r.rtsp = true;  changed = true; }
                if (!path.isEmpty() && r.path.isEmpty()) {
                    r.path = path;
                    r.rtspUrl = QStringLiteral("rtsp://%1:%2%3").arg(ip).arg(port).arg(path);
                    changed = true;
                }
                if (changed) {
                    // обновить подпись в списке
                    const int row = m_list->currentRow(); Q_UNUSED(row);
                    for (int i = 0; i < m_list->count(); ++i) {
                        if (m_list->item(i)->data(Qt::UserRole).toString()
                            == QStringLiteral("%1:%2").arg(ip).arg(port)) {
                            QString label = QStringLiteral("%1:%2").arg(ip).arg(port);
                            if (!r.path.isEmpty()) label += QStringLiteral("  %1").arg(r.path);
                            if (r.onvif) label += QStringLiteral("  [ONVIF]");
                            if (r.rtsp)  label += QStringLiteral("  [RTSP ✓]");
                            m_list->item(i)->setText(label);
                        }
                    }
                }
                break;
            }
        }
        return;
    }
    m_seen.insert(key);

    ScanResult res;
    res.ip = ip;
    res.port = port;
    res.onvif = onvif;
    res.rtsp = rtsp;
    res.path = path;
    if (!path.isEmpty())
        res.rtspUrl = QStringLiteral("rtsp://%1:%2%3").arg(ip).arg(port).arg(path);
    m_results.append(res);

    QString label = QStringLiteral("%1:%2").arg(ip).arg(port);
    if (!path.isEmpty()) label += QStringLiteral("  %1").arg(path);
    if (onvif) label += QStringLiteral("  [ONVIF]");
    if (rtsp)  label += QStringLiteral("  [RTSP ✓]");
    auto *item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, key);
    m_list->addItem(item);
    m_status->setText(QStringLiteral("Найдено: %1").arg(m_results.size()));
}

#if defined(TEST_RTSPCHECK) && !defined(FORCE_SCANNER_MOC)
#else
#include "camerascanner.moc"
#endif
