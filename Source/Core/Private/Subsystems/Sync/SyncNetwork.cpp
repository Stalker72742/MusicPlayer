//
// Created by Stalker7274 on 21.11.2026.
//

#include "SyncNetwork.h"

#include "SyncCrypto.h"
#include "SyncData.h"
#include "sync.qpb.h"

#include <QDateTime>
#include <QEventLoop>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProtobufSerializer>
#include <QTimer>
#include <QUdpSocket>
#include <QXmlStreamReader>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <optional>

namespace proto = soundlink::sync::v1;

namespace
{
    constexpr quint32 BeaconProtocol = 1;
    constexpr quint32 BeaconGroupIPv4 = 0xEFFF4D4D;
    constexpr int TickMs = 5000;
    constexpr qint64 DiscoveredExpiryMs = 20000;
    constexpr qint64 HostStickyMs = 12000;
    constexpr qint64 SignalIntervalMs = 1000;
    constexpr qsizetype MaxBeaconSize = 512;
    constexpr qsizetype MaxNameLength = 64;
    constexpr int MaxDatagramsPerRead = 64;

    constexpr quint32 StunMagicCookie = 0x2112A442;
    constexpr int StunFirstWaitMs = 1000;
    constexpr int StunSecondWaitMs = 2000;
    constexpr int StunRecheckMs = 10 * 60 * 1000;

    constexpr quint32 SsdpGroupIPv4 = 0xEFFFFFFA;
    constexpr quint16 SsdpPort = 1900;
    constexpr int SsdpWindowMs = 3000;
    constexpr qsizetype MaxSsdpLocations = 8;
    constexpr int HttpTimeoutMs = 5000;
    constexpr qint64 MaxDescriptionSize = 512 * 1024;
    constexpr int MappingLeaseSeconds = 3600;
    constexpr int MappingRefreshMs = 30 * 60 * 1000;
    constexpr int MaxMappingAttempts = 10;
    constexpr int RouterWaitMs = 1000;

    constexpr qsizetype MaxEndpoints = 16;
    constexpr int MaxIpv6Endpoints = 2;

    struct StunServerInfo
    {
        const char* host;
        quint16 port;
    };

    constexpr std::array<StunServerInfo, 2> StunServers{{{"stun.l.google.com", 19302}, {"stun.cloudflare.com", 3478}}};

    struct Range
    {
        quint32 network;
        int prefix;
    };

    constexpr std::array<Range, 13> NonPublicRanges{{
        {0x00000000, 8},  {0x0A000000, 8},  {0x64400000, 10}, {0x7F000000, 8},  {0xA9FE0000, 16},
        {0xAC100000, 12}, {0xC0000000, 24}, {0xC0000200, 24}, {0xC0A80000, 16}, {0xC6120000, 15},
        {0xC6336400, 24}, {0xCB007100, 24}, {0xE0000000, 3},
    }};

    const QStringList& SearchTargets()
    {
        static const QStringList targets{
            QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:1"),
            QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:2"),
            QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:1"),
            QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:2"),
            QStringLiteral("urn:schemas-upnp-org:service:WANPPPConnection:1"),
        };
        return targets;
    }

    const QStringList& GatewayServices()
    {
        static const QStringList services{
            QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:2"),
            QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:1"),
            QStringLiteral("urn:schemas-upnp-org:service:WANPPPConnection:1"),
        };
        return services;
    }

    qint64 NowMs()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }

    std::optional<quint32> ToIPv4(const QHostAddress& address)
    {
        bool bOk = false;
        const quint32 ipv4 = address.toIPv4Address(&bOk);
        if (!bOk)
        {
            return std::nullopt;
        }
        return ipv4;
    }

    quint32 Mask(int prefix)
    {
        if (prefix <= 0)
        {
            return 0;
        }
        if (prefix >= 32)
        {
            return 0xFFFFFFFFu;
        }
        return ~quint32(0) << (32 - prefix);
    }

    bool InRange(quint32 address, quint32 network, int prefix)
    {
        return (address & Mask(prefix)) == network;
    }

    bool IsLanIPv4(quint32 address)
    {
        return InRange(address, 0x0A000000, 8) || InRange(address, 0xAC100000, 12) || InRange(address, 0xC0A80000, 16)
               || InRange(address, 0xA9FE0000, 16);
    }

    bool IsPublicIPv4(quint32 address)
    {
        return std::none_of(NonPublicRanges.cbegin(), NonPublicRanges.cend(),
                            [address](const Range& range) { return InRange(address, range.network, range.prefix); });
    }

    bool IsTunnelIPv6(const QHostAddress& address)
    {
        const Q_IPV6ADDR bytes = address.toIPv6Address();
        const bool bTeredo = bytes[0] == 0x20 && bytes[1] == 0x01 && bytes[2] == 0x00 && bytes[3] == 0x00;
        const bool b6to4 = bytes[0] == 0x20 && bytes[1] == 0x02;
        return bTeredo || b6to4;
    }

    QString Endpoint(const QHostAddress& address, quint16 port)
    {
        if (address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            return QStringLiteral("[%1]:%2").arg(address.toString()).arg(port);
        }
        return QStringLiteral("%1:%2").arg(address.toString()).arg(port);
    }

    bool HasIPv4(const QNetworkInterface& iface)
    {
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        return std::any_of(entries.cbegin(), entries.cend(), [](const QNetworkAddressEntry& entry)
        {
            return entry.ip().protocol() == QAbstractSocket::IPv4Protocol;
        });
    }

    bool CanMulticast(const QNetworkInterface& iface)
    {
        return iface.flags().testFlag(QNetworkInterface::CanMulticast) && HasIPv4(iface);
    }

    QList<QNetworkInterface> UsableInterfaces()
    {
        QList<QNetworkInterface> usable;
        const QList<QNetworkInterface> all = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& iface : all)
        {
            const QNetworkInterface::InterfaceFlags flags = iface.flags();
            if (iface.isValid() && flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning)
                && !flags.testFlag(QNetworkInterface::IsLoopBack))
            {
                usable.append(iface);
            }
        }
        return usable;
    }

    QString Signature(const QList<QNetworkInterface>& interfaces)
    {
        QStringList parts;
        for (const QNetworkInterface& iface : interfaces)
        {
            const QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry& entry : entries)
            {
                parts.append(QStringLiteral("%1/%2/%3").arg(iface.index()).arg(entry.ip().toString()).arg(entry.prefixLength()));
            }
        }
        parts.sort();
        return parts.join(u';');
    }

    QString CleanName(const QString& name)
    {
        QString clean;
        clean.reserve(name.size());
        for (const QChar c : name)
        {
            const QChar::Category category = c.category();
            if (category != QChar::Other_Control && category != QChar::Other_Format)
            {
                clean.append(c);
            }
        }
        clean = clean.simplified().left(MaxNameLength);
        if (!clean.isEmpty() && clean.back().isHighSurrogate())
        {
            clean.chop(1);
        }
        return clean;
    }

    std::optional<quint32> ParseStunResponse(const QByteArray& data, const QByteArray& transaction)
    {
        if (data.size() < 20 || transaction.size() != 12)
        {
            return std::nullopt;
        }
        const auto* bytes = reinterpret_cast<const uchar*>(data.constData());
        const qsizetype length = qFromBigEndian<quint16>(bytes + 2);
        if (qFromBigEndian<quint16>(bytes) != 0x0101 || qFromBigEndian<quint32>(bytes + 4) != StunMagicCookie
            || 20 + length > data.size() || data.mid(8, 12) != transaction)
        {
            return std::nullopt;
        }
        std::optional<quint32> plain;
        const qsizetype end = 20 + length;
        qsizetype offset = 20;
        while (offset + 4 <= end)
        {
            const quint16 attribute = qFromBigEndian<quint16>(bytes + offset);
            const qsizetype size = qFromBigEndian<quint16>(bytes + offset + 2);
            const qsizetype value = offset + 4;
            if (value + size > end)
            {
                break;
            }
            if ((attribute == 0x0020 || attribute == 0x0001) && size >= 8 && bytes[value + 1] == 0x01)
            {
                const quint32 address = qFromBigEndian<quint32>(bytes + value + 4);
                if (attribute == 0x0020)
                {
                    return address ^ StunMagicCookie;
                }
                plain = address;
            }
            offset = value + ((size + 3) & ~qsizetype(3));
        }
        return plain;
    }

    QByteArray SearchRequest(const QString& target)
    {
        return QByteArrayLiteral("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ")
               + target.toLatin1() + QByteArrayLiteral("\r\n\r\n");
    }

    bool IsRouterUrl(const QUrl& url)
    {
        if (!url.isValid() || url.scheme() != u"http" || !url.userInfo().isEmpty())
        {
            return false;
        }
        const std::optional<quint32> host = ToIPv4(QHostAddress(url.host()));
        return host && IsLanIPv4(*host);
    }

    QUrl ParseSsdpLocation(const QByteArray& response)
    {
        const QList<QByteArray> lines = response.split('\n');
        if (lines.isEmpty())
        {
            return {};
        }
        const QList<QByteArray> status = lines.first().simplified().split(' ');
        if (status.size() < 2 || !status.at(0).toUpper().startsWith("HTTP/1.") || status.at(1) != "200")
        {
            return {};
        }
        for (const QByteArray& line : lines)
        {
            const qsizetype colon = line.indexOf(':');
            if (colon <= 0 || line.left(colon).trimmed().toLower() != "location")
            {
                continue;
            }
            const QUrl url(QString::fromLatin1(line.mid(colon + 1).trimmed()), QUrl::StrictMode);
            return IsRouterUrl(url) ? url : QUrl();
        }
        return {};
    }

    struct ServiceInfo
    {
        QString type;
        QUrl control;
    };

    QList<ServiceInfo> ParseDescription(const QByteArray& xml, const QUrl& location)
    {
        QList<ServiceInfo> services;
        QXmlStreamReader reader(xml);
        QString urlBase;
        QString type;
        QString control;
        bool bInService = false;
        while (!reader.atEnd() && !reader.hasError())
        {
            const QXmlStreamReader::TokenType token = reader.readNext();
            if (token == QXmlStreamReader::StartElement)
            {
                const QStringView name = reader.name();
                if (name == u"service")
                {
                    bInService = true;
                    type.clear();
                    control.clear();
                }
                else if (name == u"URLBase")
                {
                    urlBase = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                }
                else if (bInService && name == u"serviceType")
                {
                    type = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                }
                else if (bInService && name == u"controlURL")
                {
                    control = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                }
            }
            else if (token == QXmlStreamReader::EndElement && bInService && reader.name() == u"service")
            {
                bInService = false;
                if (!GatewayServices().contains(type) || control.isEmpty())
                {
                    continue;
                }
                const QUrl base = urlBase.isEmpty() ? location : QUrl(urlBase);
                const QUrl url = base.resolved(QUrl(control));
                if (url.host() == location.host() && IsRouterUrl(url))
                {
                    services.append({type, url});
                }
            }
        }
        return services;
    }

    QString XmlValue(const QByteArray& xml, QStringView name)
    {
        QXmlStreamReader reader(xml);
        while (!reader.atEnd() && !reader.hasError())
        {
            if (reader.readNext() == QXmlStreamReader::StartElement && reader.name() == name)
            {
                return reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            }
        }
        return {};
    }
}

SyncNetwork::SyncNetwork(QObject* parent)
    : QObject(parent)
{
    tickTimer = new QTimer(this);
    tickTimer->setInterval(TickMs);
    connect(tickTimer, &QTimer::timeout, this, &SyncNetwork::Tick);

    stunTimer = new QTimer(this);
    stunTimer->setSingleShot(true);
    connect(stunTimer, &QTimer::timeout, this, &SyncNetwork::OnStunTimeout);

    stunRecheckTimer = new QTimer(this);
    stunRecheckTimer->setInterval(StunRecheckMs);
    connect(stunRecheckTimer, &QTimer::timeout, this, &SyncNetwork::StartStun);

    ssdpTimer = new QTimer(this);
    ssdpTimer->setSingleShot(true);
    connect(ssdpTimer, &QTimer::timeout, this, &SyncNetwork::FinishSsdp);

    refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    connect(refreshTimer, &QTimer::timeout, this, &SyncNetwork::StartUpnp);
}

SyncNetwork::~SyncNetwork()
{
    blockSignals(true);
    Stop();
}

void SyncNetwork::Start(const QString& id, quint16 port, bool bUpnp)
{
    Stop();
    deviceId = id;
    listenPort = port;
    bUpnpEnabled = bUpnp;
    bRunning = true;
    interfaces = UsableInterfaces();
    interfaceSignature = Signature(interfaces);
    OpenBeaconSocket();
    tickTimer->start();
    SendBeacon();
    StartStun();
    if (bUpnpEnabled)
    {
        StartUpnp();
    }
    UpdateReachability();
}

void SyncNetwork::Stop(bool bWaitForRouter)
{
    if (!bRunning)
    {
        return;
    }
    bRunning = false;
    tickTimer->stop();
    CloseBeaconSocket();
    StopStun();
    StopUpnp(bWaitForRouter);
    hostSeenMs.clear();
    signalledMs.clear();
    detectedAddress.clear();
    reachability = Reachability::Unknown;
    publicEndpoint.clear();
    const bool bHadDevices = !discovered.isEmpty();
    discovered.clear();
    if (bHadDevices)
    {
        emit discoveredChanged();
    }
    emit networkChanged();
}

void SyncNetwork::SetBeacon(const QString& name, bool bPairing)
{
    const QString clean = CleanName(name);
    if (clean == beaconName && bPairing == bBeaconPairing)
    {
        return;
    }
    beaconName = clean;
    bBeaconPairing = bPairing;
    SendBeacon();
}

void SyncNetwork::SetUpnp(bool bEnabled)
{
    if (bEnabled == bUpnpEnabled)
    {
        return;
    }
    bUpnpEnabled = bEnabled;
    if (!bRunning)
    {
        return;
    }
    if (bEnabled)
    {
        StartUpnp();
    }
    else
    {
        StopUpnp(false);
    }
    UpdateReachability();
}

void SyncNetwork::Recheck()
{
    if (!bRunning)
    {
        return;
    }
    StartStun();
    if (bUpnpEnabled)
    {
        StartUpnp();
    }
    UpdateReachability();
}

QString SyncNetwork::GetDetectedAddress() const
{
    return detectedAddress.isNull() ? QString() : detectedAddress.toString();
}

QStringList SyncNetwork::LocalEndpoints() const
{
    QStringList endpoints;
    if (listenPort == 0)
    {
        return endpoints;
    }
    for (const QNetworkInterface& iface : interfaces)
    {
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries)
        {
            const std::optional<quint32> ipv4 = ToIPv4(entry.ip());
            if (ipv4 && entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !InRange(*ipv4, 0xA9FE0000, 16))
            {
                endpoints.append(Endpoint(entry.ip(), listenPort));
            }
        }
    }
    endpoints.removeDuplicates();
    return endpoints;
}

QStringList SyncNetwork::Endpoints() const
{
    QStringList endpoints;
    if (!publicEndpoint.isEmpty())
    {
        endpoints.append(publicEndpoint);
    }
    endpoints.append(LocalEndpoints());
    int ipv6 = 0;
    for (const QNetworkInterface& iface : interfaces)
    {
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries)
        {
            const QHostAddress ip = entry.ip();
            if (listenPort == 0 || ipv6 >= MaxIpv6Endpoints || ip.protocol() != QAbstractSocket::IPv6Protocol || !ip.isGlobal()
                || ip.isUniqueLocalUnicast() || ip.isLinkLocal() || entry.isTemporary() || IsTunnelIPv6(ip))
            {
                continue;
            }
            endpoints.append(Endpoint(ip, listenPort));
            ++ipv6;
        }
    }
    endpoints.removeDuplicates();
    while (endpoints.size() > MaxEndpoints)
    {
        endpoints.removeLast();
    }
    return endpoints;
}

void SyncNetwork::Tick()
{
    RefreshInterfaces();
    SendBeacon();
    ExpireDiscovered();
}

void SyncNetwork::RefreshInterfaces()
{
    QList<QNetworkInterface> current = UsableInterfaces();
    QString signature = Signature(current);
    if (signature == interfaceSignature)
    {
        return;
    }
    interfaces = std::move(current);
    interfaceSignature = std::move(signature);
    OpenBeaconSocket();
    Recheck();
}

void SyncNetwork::OpenBeaconSocket()
{
    CloseBeaconSocket();
    beaconSocket = new QUdpSocket(this);
    bBeaconsBound = beaconSocket->bind(QHostAddress::AnyIPv4, DiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bBeaconsBound)
    {
        qWarning() << "Sync: beacon port" << DiscoveryPort << "is taken:" << beaconSocket->errorString();
        beaconSocket->deleteLater();
        beaconSocket = new QUdpSocket(this);
        if (!beaconSocket->bind(QHostAddress::AnyIPv4, 0))
        {
            qWarning() << "Sync: cannot open a beacon socket:" << beaconSocket->errorString();
            beaconSocket->deleteLater();
            beaconSocket = nullptr;
            return;
        }
    }
    beaconSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
    beaconSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    if (!bBeaconsBound)
    {
        return;
    }
    const QHostAddress group(BeaconGroupIPv4);
    for (const QNetworkInterface& iface : std::as_const(interfaces))
    {
        if (CanMulticast(iface))
        {
            beaconSocket->joinMulticastGroup(group, iface);
        }
    }
    connect(beaconSocket, &QUdpSocket::readyRead, this, &SyncNetwork::ReadBeacons);
}

void SyncNetwork::CloseBeaconSocket()
{
    bBeaconsBound = false;
    if (!beaconSocket)
    {
        return;
    }
    beaconSocket->disconnect(this);
    beaconSocket->close();
    beaconSocket->deleteLater();
    beaconSocket = nullptr;
}

QByteArray SyncNetwork::BeaconPacket() const
{
    proto::Beacon beacon;
    beacon.setProtocol(BeaconProtocol);
    beacon.setDeviceId(deviceId);
    beacon.setDeviceName(beaconName);
    beacon.setPlatform(proto::PlatformGadget::Platform::PLATFORM_WINDOWS);
    beacon.setPort(listenPort);
    beacon.setPairing(bBeaconPairing);
    QProtobufSerializer serializer;
    return QByteArrayLiteral("SLB1") + beacon.serialize(&serializer);
}

void SyncNetwork::SendBeacon()
{
    if (!bRunning || !beaconSocket || listenPort == 0 || deviceId.isEmpty())
    {
        return;
    }
    const QByteArray packet = BeaconPacket();
    const QHostAddress group(BeaconGroupIPv4);
    for (const QNetworkInterface& iface : std::as_const(interfaces))
    {
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries)
        {
            const QHostAddress broadcast = entry.broadcast();
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !broadcast.isNull() && broadcast != entry.ip())
            {
                beaconSocket->writeDatagram(packet, broadcast, DiscoveryPort);
            }
        }
        if (CanMulticast(iface))
        {
            beaconSocket->setMulticastInterface(iface);
            beaconSocket->writeDatagram(packet, group, DiscoveryPort);
        }
    }
}

void SyncNetwork::ReadBeacons()
{
    QUdpSocket* const socket = beaconSocket;
    for (int i = 0; i < MaxDatagramsPerRead && socket && socket == beaconSocket && socket->hasPendingDatagrams(); ++i)
    {
        const QNetworkDatagram datagram = socket->receiveDatagram(MaxBeaconSize + 1);
        if (!datagram.isValid())
        {
            break;
        }
        HandleBeacon(datagram.data(), datagram.senderAddress());
    }
}

void SyncNetwork::HandleBeacon(const QByteArray& packet, const QHostAddress& sender)
{
    const QByteArray magic = QByteArrayLiteral("SLB1");
    const std::optional<quint32> host = ToIPv4(sender);
    if (packet.size() > MaxBeaconSize || !packet.startsWith(magic) || !host)
    {
        return;
    }
    proto::Beacon beacon;
    QProtobufSerializer serializer;
    if (!beacon.deserialize(&serializer, QByteArrayView(packet).sliced(magic.size())))
    {
        return;
    }
    const QString id = beacon.deviceId();
    if (beacon.protocol() != BeaconProtocol || !Sync::IsDeviceId(id) || id == deviceId || beacon.port() == 0 || beacon.port() > 65535)
    {
        return;
    }

    const qint64 now = NowMs();
    Discovered device;
    device.deviceId = id;
    device.name = CleanName(beacon.deviceName());
    device.platform = static_cast<int>(beacon.platform());
    device.host = QHostAddress(*host).toString();
    device.port = quint16(beacon.port());
    device.bPairing = beacon.pairing();
    device.lastSeenMs = now;

    const auto known = discovered.constFind(id);
    if (known != discovered.cend() && known->host != device.host && now - hostSeenMs.value(id) < HostStickyMs)
    {
        device.host = known->host;
    }
    else
    {
        hostSeenMs.insert(id, now);
    }
    const bool bChanged = known == discovered.cend() || known->name != device.name || known->host != device.host
                          || known->port != device.port || known->bPairing != device.bPairing || known->platform != device.platform;
    discovered.insert(id, device);
    if (bChanged)
    {
        emit discoveredChanged();
    }
    if (bChanged || now - signalledMs.value(id) >= SignalIntervalMs)
    {
        signalledMs.insert(id, now);
        emit beaconReceived(device);
    }
}

void SyncNetwork::ExpireDiscovered()
{
    const qint64 now = NowMs();
    bool bChanged = false;
    for (auto it = discovered.begin(); it != discovered.end();)
    {
        if (now - it->lastSeenMs > DiscoveredExpiryMs)
        {
            hostSeenMs.remove(it.key());
            signalledMs.remove(it.key());
            it = discovered.erase(it);
            bChanged = true;
        }
        else
        {
            ++it;
        }
    }
    if (bChanged)
    {
        emit discoveredChanged();
    }
}

bool SyncNetwork::IsLocalAddress(quint32 ipv4) const
{
    for (const QNetworkInterface& iface : interfaces)
    {
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries)
        {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && ToIPv4(entry.ip()) == ipv4)
            {
                return true;
            }
        }
    }
    return false;
}

QHostAddress SyncNetwork::LocalAddressFor(quint32 router) const
{
    for (const QNetworkInterface& iface : interfaces)
    {
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries)
        {
            const std::optional<quint32> ip = ToIPv4(entry.ip());
            const int prefix = entry.prefixLength();
            if (ip && entry.ip().protocol() == QAbstractSocket::IPv4Protocol && prefix > 0 && prefix <= 32
                && InRange(router, *ip & Mask(prefix), prefix))
            {
                return entry.ip();
            }
        }
    }
    return {};
}

void SyncNetwork::StartStun()
{
    if (!bRunning)
    {
        return;
    }
    ++stunGeneration;
    stunTimer->stop();
    stunTransaction.clear();
    stunServerIndex = 0;
    stunRecheckTimer->start();
    LookupStunServer();
}

void SyncNetwork::StopStun()
{
    ++stunGeneration;
    stunTimer->stop();
    stunRecheckTimer->stop();
    stunTransaction.clear();
    if (stunSocket)
    {
        stunSocket->disconnect(this);
        stunSocket->close();
        stunSocket->deleteLater();
        stunSocket = nullptr;
    }
}

void SyncNetwork::LookupStunServer()
{
    if (stunServerIndex >= int(StunServers.size()))
    {
        FinishStun({});
        return;
    }
    const quint64 generation = stunGeneration;
    QHostInfo::lookupHost(QString::fromLatin1(StunServers[stunServerIndex].host), this, [this, generation](const QHostInfo& info)
    {
        if (generation != stunGeneration || !bRunning)
        {
            return;
        }
        const QList<QHostAddress> addresses = info.addresses();
        for (const QHostAddress& address : addresses)
        {
            if (address.protocol() == QAbstractSocket::IPv4Protocol)
            {
                BeginStunExchange(address);
                return;
            }
        }
        ++stunServerIndex;
        LookupStunServer();
    });
}

void SyncNetwork::BeginStunExchange(const QHostAddress& server)
{
    if (!stunSocket)
    {
        stunSocket = new QUdpSocket(this);
        if (!stunSocket->bind(QHostAddress::AnyIPv4, 0))
        {
            FinishStun({});
            return;
        }
        connect(stunSocket, &QUdpSocket::readyRead, this, &SyncNetwork::ReadStun);
    }
    stunServer = server;
    stunServerPort = StunServers[stunServerIndex].port;
    stunTransaction = SyncCrypto::RandomBytes(12);
    stunAttempts = 0;
    SendStunRequest();
}

void SyncNetwork::SendStunRequest()
{
    QByteArray request(20, '\0');
    qToBigEndian<quint16>(0x0001, request.data());
    qToBigEndian<quint16>(0, request.data() + 2);
    qToBigEndian<quint32>(StunMagicCookie, request.data() + 4);
    std::copy(stunTransaction.cbegin(), stunTransaction.cend(), request.begin() + 8);
    stunSocket->writeDatagram(request, stunServer, stunServerPort);
    ++stunAttempts;
    stunTimer->start(stunAttempts == 1 ? StunFirstWaitMs : StunSecondWaitMs);
}

void SyncNetwork::ReadStun()
{
    for (int i = 0; i < MaxDatagramsPerRead && stunSocket && stunSocket->hasPendingDatagrams(); ++i)
    {
        const QNetworkDatagram datagram = stunSocket->receiveDatagram(1500);
        if (!datagram.isValid())
        {
            break;
        }
        if (stunTransaction.isEmpty() || ToIPv4(datagram.senderAddress()) != ToIPv4(stunServer))
        {
            continue;
        }
        if (const std::optional<quint32> mapped = ParseStunResponse(datagram.data(), stunTransaction))
        {
            FinishStun(QHostAddress(*mapped));
            return;
        }
    }
}

void SyncNetwork::OnStunTimeout()
{
    if (stunTransaction.isEmpty() || !stunSocket)
    {
        return;
    }
    if (stunAttempts < 2)
    {
        SendStunRequest();
        return;
    }
    stunTransaction.clear();
    ++stunServerIndex;
    LookupStunServer();
}

void SyncNetwork::FinishStun(const QHostAddress& mapped)
{
    stunTimer->stop();
    stunTransaction.clear();
    if (stunSocket)
    {
        stunSocket->disconnect(this);
        stunSocket->close();
        stunSocket->deleteLater();
        stunSocket = nullptr;
    }
    detectedAddress = mapped;
    UpdateReachability();
}

QNetworkAccessManager* SyncNetwork::Http()
{
    if (!http)
    {
        http = new QNetworkAccessManager(this);
        http->setProxy(QNetworkProxy::NoProxy);
        http->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    }
    return http;
}

void SyncNetwork::StartUpnp()
{
    if (!bRunning || !bUpnpEnabled)
    {
        return;
    }
    ++upnpGeneration;
    ssdpTimer->stop();
    refreshTimer->stop();
    CloseSsdpSocket();
    ssdpLocations.clear();
    gateways.clear();
    gatewayIndex = 0;
    if (!bMapped)
    {
        upnpState = UpnpState::Searching;
    }
    ssdpSocket = new QUdpSocket(this);
    if (!ssdpSocket->bind(QHostAddress::AnyIPv4, 0))
    {
        FailUpnp();
        return;
    }
    ssdpSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 2);
    connect(ssdpSocket, &QUdpSocket::readyRead, this, &SyncNetwork::ReadSsdp);
    const QHostAddress group(SsdpGroupIPv4);
    for (const QNetworkInterface& iface : std::as_const(interfaces))
    {
        if (!CanMulticast(iface))
        {
            continue;
        }
        ssdpSocket->setMulticastInterface(iface);
        for (const QString& target : SearchTargets())
        {
            ssdpSocket->writeDatagram(SearchRequest(target), group, SsdpPort);
        }
    }
    ssdpTimer->start(SsdpWindowMs);
}

void SyncNetwork::StopUpnp(bool bWait)
{
    ++upnpGeneration;
    ssdpTimer->stop();
    refreshTimer->stop();
    CloseSsdpSocket();
    if (bMapped)
    {
        const QString arguments = QStringLiteral("<NewRemoteHost></NewRemoteHost><NewExternalPort>%1</NewExternalPort>"
                                                 "<NewProtocol>TCP</NewProtocol>")
                                      .arg(mappedPort);
        QNetworkReply* reply = Soap(mapping, QStringLiteral("DeletePortMapping"), arguments);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
        if (bWait)
        {
            QEventLoop loop;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(RouterWaitMs, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
    }
    bMapped = false;
    mapping = {};
    mappedPort = 0;
    externalAddress.clear();
    ssdpLocations.clear();
    gateways.clear();
    upnpState = UpnpState::Off;
}

void SyncNetwork::CloseSsdpSocket()
{
    if (!ssdpSocket)
    {
        return;
    }
    ssdpSocket->disconnect(this);
    ssdpSocket->close();
    ssdpSocket->deleteLater();
    ssdpSocket = nullptr;
}

void SyncNetwork::ReadSsdp()
{
    for (int i = 0; i < MaxDatagramsPerRead && ssdpSocket && ssdpSocket->hasPendingDatagrams(); ++i)
    {
        const QNetworkDatagram datagram = ssdpSocket->receiveDatagram(4096);
        if (!datagram.isValid())
        {
            break;
        }
        const std::optional<quint32> sender = ToIPv4(datagram.senderAddress());
        if (!sender || !IsLanIPv4(*sender) || ssdpLocations.size() >= MaxSsdpLocations)
        {
            continue;
        }
        const QUrl location = ParseSsdpLocation(datagram.data());
        if (location.isValid() && !ssdpLocations.contains(location))
        {
            ssdpLocations.append(location);
        }
    }
}

void SyncNetwork::FinishSsdp()
{
    CloseSsdpSocket();
    if (ssdpLocations.isEmpty())
    {
        FailUpnp();
        return;
    }
    FetchDescription(0);
}

void SyncNetwork::FetchDescription(int index)
{
    if (index >= ssdpLocations.size())
    {
        std::stable_sort(gateways.begin(), gateways.end(), [](const Gateway& a, const Gateway& b)
        {
            return GatewayServices().indexOf(a.serviceType) < GatewayServices().indexOf(b.serviceType);
        });
        TryGateway(0);
        return;
    }
    const QUrl location = ssdpLocations.at(index);
    QNetworkRequest request(location);
    request.setTransferTimeout(HttpTimeoutMs);
    request.setRawHeader("Connection", "close");
    QNetworkReply* reply = Http()->get(request);
    const quint64 generation = upnpGeneration;
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64)
    {
        if (received > MaxDescriptionSize)
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, index, location]
    {
        reply->deleteLater();
        if (generation != upnpGeneration)
        {
            return;
        }
        const std::optional<quint32> router = ToIPv4(QHostAddress(location.host()));
        const QHostAddress local = router ? LocalAddressFor(*router) : QHostAddress();
        if (reply->error() == QNetworkReply::NoError && !local.isNull())
        {
            const QList<ServiceInfo> services = ParseDescription(reply->readAll(), location);
            for (const ServiceInfo& service : services)
            {
                const bool bKnown = std::any_of(gateways.cbegin(), gateways.cend(), [&service](const Gateway& gateway)
                {
                    return gateway.controlUrl == service.control;
                });
                if (!bKnown)
                {
                    gateways.append({service.control, service.type, local});
                }
            }
        }
        FetchDescription(index + 1);
    });
}

void SyncNetwork::TryGateway(int index)
{
    if (index >= gateways.size())
    {
        FailUpnp();
        return;
    }
    gatewayIndex = index;
    const quint64 generation = upnpGeneration;
    QNetworkReply* reply = Soap(gateways.at(index), QStringLiteral("GetExternalIPAddress"), {});
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, index]
    {
        reply->deleteLater();
        if (generation != upnpGeneration)
        {
            return;
        }
        const QString text = reply->error() == QNetworkReply::NoError ? XmlValue(reply->readAll(), u"NewExternalIPAddress") : QString();
        const std::optional<quint32> external = ToIPv4(QHostAddress(text));
        if (!external || *external == 0)
        {
            TryGateway(index + 1);
            return;
        }
        gatewayExternal = QHostAddress(*external);
        RequestMapping(0, false);
    });
}

void SyncNetwork::RequestMapping(int attempt, bool bPermanent)
{
    const Gateway gateway = gateways.at(gatewayIndex);
    const int external = int(listenPort) + attempt;
    if (external > 65535)
    {
        TryGateway(gatewayIndex + 1);
        return;
    }
    const QString arguments = QStringLiteral("<NewRemoteHost></NewRemoteHost>"
                                             "<NewExternalPort>%1</NewExternalPort>"
                                             "<NewProtocol>TCP</NewProtocol>"
                                             "<NewInternalPort>%2</NewInternalPort>"
                                             "<NewInternalClient>%3</NewInternalClient>"
                                             "<NewEnabled>1</NewEnabled>"
                                             "<NewPortMappingDescription>SoundLink sync</NewPortMappingDescription>"
                                             "<NewLeaseDuration>%4</NewLeaseDuration>")
                                  .arg(external)
                                  .arg(listenPort)
                                  .arg(gateway.localAddress.toString())
                                  .arg(bPermanent ? 0 : MappingLeaseSeconds);
    const quint64 generation = upnpGeneration;
    QNetworkReply* reply = Soap(gateway, QStringLiteral("AddPortMapping"), arguments);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, gateway, external, attempt, bPermanent]
    {
        reply->deleteLater();
        if (generation != upnpGeneration)
        {
            return;
        }
        const QByteArray body = reply->readAll();
        if (reply->error() == QNetworkReply::NoError)
        {
            bMapped = true;
            mapping = gateway;
            mappedPort = quint16(external);
            externalAddress = gatewayExternal;
            upnpState = UpnpState::Mapped;
            refreshTimer->start(MappingRefreshMs);
            UpdateReachability();
            return;
        }
        const int code = XmlValue(body, u"errorCode").toInt();
        if (code == 725 && !bPermanent)
        {
            RequestMapping(attempt, true);
        }
        else if (code == 718 && attempt < MaxMappingAttempts)
        {
            RequestMapping(attempt + 1, bPermanent);
        }
        else
        {
            qWarning() << "Sync: the router refused the port mapping, UPnP error" << code << reply->errorString();
            TryGateway(gatewayIndex + 1);
        }
    });
}

void SyncNetwork::FailUpnp()
{
    ssdpTimer->stop();
    CloseSsdpSocket();
    bMapped = false;
    mapping = {};
    mappedPort = 0;
    externalAddress.clear();
    upnpState = UpnpState::Failed;
    refreshTimer->start(MappingRefreshMs);
    UpdateReachability();
}

QNetworkReply* SyncNetwork::Soap(const Gateway& gateway, const QString& action, const QString& arguments)
{
    QNetworkRequest request(gateway.controlUrl);
    request.setTransferTimeout(HttpTimeoutMs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml; charset=\"utf-8\""));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1#%2\"").arg(gateway.serviceType, action).toLatin1());
    request.setRawHeader("Connection", "close");
    const QString body = QStringLiteral("<?xml version=\"1.0\"?>\r\n"
                                        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                                        "<s:Body><u:%1 xmlns:u=\"%2\">%3</u:%1></s:Body></s:Envelope>\r\n")
                             .arg(action, gateway.serviceType, arguments);
    return Http()->post(request, body.toUtf8());
}

void SyncNetwork::UpdateReachability()
{
    const std::optional<quint32> detected = ToIPv4(detectedAddress);
    const std::optional<quint32> external = bMapped ? ToIPv4(externalAddress) : std::optional<quint32>();
    Reachability next = Reachability::Unknown;
    QString endpoint;
    if (detected && IsPublicIPv4(*detected) && IsLocalAddress(*detected))
    {
        next = Reachability::Public;
        endpoint = Endpoint(QHostAddress(*detected), listenPort);
    }
    else if (external && IsPublicIPv4(*external) && (!detected || *detected == *external))
    {
        next = Reachability::Mapped;
        endpoint = Endpoint(QHostAddress(*external), mappedPort);
    }
    else if (detected || external)
    {
        next = Reachability::Private;
    }
    reachability = next;
    publicEndpoint = endpoint;
    emit networkChanged();
}
