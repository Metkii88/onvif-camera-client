#include "onvifdiscovery.h"

#include <QNetworkInterface>
#include <QRegularExpression>
#include <QDebug>

static const quint16 WSD_PORT = 3700;
static const char MULTICAST_ADDR[] = "239.255.255.250";

OnvifDiscovery::OnvifDiscovery(QObject *parent)
    : QObject(parent)
{
    m_socket = new QUdpSocket(this);
    m_socket->bind(QHostAddress::AnyIPv4, 0,
                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(m_socket, &QUdpSocket::readyRead, this, &OnvifDiscovery::onReadyRead);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &OnvifDiscovery::onTimeout);
}

void OnvifDiscovery::start(int durationMs)
{
    m_seen.clear();
    // Уникальный MatchId для корреляции ответов
    m_matchId = QStringLiteral("urn:uuid:%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    sendProbe();
    m_timer->start(durationMs);
}

void OnvifDiscovery::stop()
{
    m_timer->stop();
}

void OnvifDiscovery::sendProbe()
{
    // Стандартный WS-Discovery Probe на ONVIF-тип сетевого видео передатчика
    QByteArray probe = QByteArray(R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
            xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing"
            xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
            xmlns:dn="http://www.onvif.org/ver10/network/wsdl">
  <s:Header>
    <a:Action s:mustUnderstand="1">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action>
    <a:MessageID>urn:uuid:MATCHID</a:MessageID>
    <a:ReplyTo><a:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address></a:ReplyTo>
    <a:To s:mustUnderstand="1">urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To>
    <d:Types>dn:NetworkVideoTransmitter</d:Types>
  </s:Header>
  <s:Body/>
</s:Envelope>)")
        .replace("MATCHID", m_matchId.toUtf8());

    // Отправка в multicast-группу
    m_socket->writeDatagram(probe, QHostAddress(MULTICAST_ADDR), WSD_PORT);

    // Дополнительно broadcast-запросы по каждому локальному интерфейсу,
    // потому что IGMP/multicast в некоторых сетях настроен плохо
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        const auto entries = iface.addressEntries();
        for (const auto &e : entries) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QHostAddress bcast(e.ip().toIPv4Address() | ~e.netmask().toIPv4Address());
                m_socket->writeDatagram(probe, bcast, WSD_PORT);
            }
        }
    }
}

void OnvifDiscovery::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        const QString xml = QString::fromUtf8(datagram);
        if (!xml.contains("ProbeMatch", Qt::CaseInsensitive))
            continue;

        // Разбираем интересующие поля регулярками — ответы камер бывают
        // разных диалектов, жёсткий XML-парсер здесь ненадёжен
        auto extract = [&xml](const QString &tag) -> QString {
            QRegularExpression re(QStringLiteral("<[^>]*%1[^>]*>(.*?)</[^>]*%1>")
                                     .arg(tag),
                                 QRegularExpression::DotMatchesEverythingOption |
                                     QRegularExpression::CaseInsensitiveOption);
            const auto m = re.match(xml);
            return m.hasMatch() ? m.captured(1).trimmed() : QString();
        };

        const QString endpoint = extract(QStringLiteral("Address"));
        const QString xaddr = extract(QStringLiteral("XAddrs"));
        const QString types = extract(QStringLiteral("Types"));
        const QString scopes = extract(QStringLiteral("Scopes"));

        if (xaddr.isEmpty() && endpoint.isEmpty())
            continue;

        // IP камеры: сначала из XAddrs, иначе из endpoint, иначе адрес отправителя
        QString camIp;
        QString camPort;
        const QString srcUrl = xaddr.isEmpty() ? endpoint : xaddr.split(' ').first();
        QRegularExpression urlRe(QStringLiteral(R"(https?://([\w\.\-]+)(?::(\d+))?)"));
        auto um = urlRe.match(srcUrl);
        if (um.hasMatch()) {
            camIp = um.captured(1);
            camPort = um.captured(2);
        }
        if (camIp.isEmpty() || camIp == QLatin1String("localhost"))
            camIp = sender.toString(); // например http://localhost:80/onvif/...

        if (camIp.isEmpty() || m_seen.contains(camIp))
            continue;
        m_seen.append(camIp);

        CameraInfo info;
        info.address = camIp;
        info.port = camPort;
        info.xaddr = srcUrl;
        info.types = types;

        // Из Scopes обычно торчат Manufacturer/Model:
        // onvif://www.onvif.org/name/IPCAM/Manufacturer/Hikvision/Model/DS-2CD...
        static const QRegularExpression manRe(
            QStringLiteral("(?:manufacturer|name)/([^/\\s]+)"),
            QRegularExpression::CaseInsensitiveOption);
        auto mm = manRe.match(scopes);
        if (mm.hasMatch())
            info.manufacturer = mm.captured(1);
        static const QRegularExpression modelRe(
            QStringLiteral("model/([^/\\s]+)"),
            QRegularExpression::CaseInsensitiveOption);
        auto md = modelRe.match(scopes);
        if (md.hasMatch())
            info.model = md.captured(1);

        // Готовый RTSP-URL: стандартный путь ONVIF-камер (часто без авторизации)
        info.rtspUrl = QStringLiteral("rtsp://%1:554/onvif1").arg(camIp);

        qDebug() << "[ONVIF] найдена камера:" << info.address << info.model << info.xaddr;
        emit cameraFound(info);
    }
}

void OnvifDiscovery::onTimeout()
{
    emit finished();
}
