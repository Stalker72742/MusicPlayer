//
// Created by Stalker7274 on 21.11.2026.
//

#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QUdpSocket;

/// @brief Finds sync peers and this device's place on the network.
///
/// LAN beacons go out every 5 s as UDP multicast (239.255.77.77) and broadcast on port 47777 of every interface;
/// the public IPv4 address comes from a STUN server; a UPnP IGD router is asked to forward a public TCP port to
/// the sync server. Everything runs on the thread that owns the object.
class SyncNetwork : public QObject
{
    Q_OBJECT

public:
    /// UDP port of LAN beacons.
    static constexpr quint16 DiscoveryPort = 47777;

    /// @brief A device heard on the local network.
    struct Discovered
    {
        QString deviceId;
        QString name;
        int platform = 0;      ///< soundlink.sync.v1.Platform.
        QString host;          ///< IPv4 address its beacons come from.
        quint16 port = 0;      ///< Port of its sync server.
        bool bPairing = false; ///< It shows a pairing code right now.
        qint64 lastSeenMs = 0;
    };

    /// @brief Whether devices on the internet can open connections to this one.
    enum class Reachability
    {
        Unknown, ///< Not checked yet, or neither the STUN server nor the router answered.
        Public,  ///< This device has a public ("white") IPv4 address itself.
        Mapped,  ///< The router forwards a public port to this device (UPnP).
        Private  ///< Behind NAT without a mapping: only the LAN and outgoing connections.
    };

    /// @brief The router port mapping.
    enum class UpnpState
    {
        Off,
        Searching,
        Mapped,
        Failed ///< No UPnP router answered, or it refused the mapping.
    };

    explicit SyncNetwork(QObject* parent = nullptr);
    ~SyncNetwork() override;

    /// @brief Starts beacons, the public address check and, with `bUpnp`, the port mapping for a server on `listenPort`.
    void Start(const QString& deviceId, quint16 listenPort, bool bUpnp);
    /// @brief Stops everything. With `bWaitForRouter` the port mapping is removed before it returns (at most ~1 s).
    void Stop(bool bWaitForRouter = false);
    bool IsRunning() const { return bRunning; }
    /// @brief What beacons tell about this device; a change goes out at once.
    void SetBeacon(const QString& name, bool bPairing);
    void SetUpnp(bool bEnabled);
    /// @brief Checks the public address and the port mapping again.
    void Recheck();

    /// @brief Devices heard during the last 20 s, by device id.
    const QHash<QString, Discovered>& GetDiscovered() const { return discovered; }
    /// @brief False when another program holds the beacon port: this device is still announced but hears nobody.
    bool IsHearingBeacons() const { return bBeaconsBound; }
    Reachability GetReachability() const { return reachability; }
    UpnpState GetUpnpState() const { return upnpState; }
    /// @brief The public IPv4 address the STUN server saw; empty when unknown.
    QString GetDetectedAddress() const;
    /// @brief "host:port" devices on the internet can connect to; empty unless Public or Mapped.
    QString GetPublicEndpoint() const { return publicEndpoint; }
    /// @brief "ip:port" of the sync server on the local networks.
    QStringList LocalEndpoints() const;
    /// @brief Everything to announce to peers, most reachable first (public, LAN IPv4, global IPv6).
    QStringList Endpoints() const;

signals:
    void discoveredChanged();
    /// @brief A beacon came from `device`; at most once a second per device unless something changed.
    void beaconReceived(const SyncNetwork::Discovered& device);
    /// @brief Reachability, the public endpoint, the port mapping or the local addresses changed.
    void networkChanged();

private:
    /// A WAN connection service of a UPnP router.
    struct Gateway
    {
        QUrl controlUrl;
        QString serviceType;
        QHostAddress localAddress; ///< This device's address on the router's subnet.
    };

    void Tick();
    void RefreshInterfaces();
    void OpenBeaconSocket();
    void CloseBeaconSocket();
    QByteArray BeaconPacket() const;
    void SendBeacon();
    void ReadBeacons();
    void HandleBeacon(const QByteArray& packet, const QHostAddress& sender);
    void ExpireDiscovered();
    bool IsLocalAddress(quint32 ipv4) const;
    QHostAddress LocalAddressFor(quint32 router) const;

    void StartStun();
    void StopStun();
    void LookupStunServer();
    void BeginStunExchange(const QHostAddress& server);
    void SendStunRequest();
    void ReadStun();
    void OnStunTimeout();
    void FinishStun(const QHostAddress& mapped);

    QNetworkAccessManager* Http();
    void StartUpnp();
    void StopUpnp(bool bWait);
    void CloseSsdpSocket();
    void ReadSsdp();
    void FinishSsdp();
    void FetchDescription(int index);
    void TryGateway(int index);
    void RequestMapping(int attempt, bool bPermanent);
    void FailUpnp();
    QNetworkReply* Soap(const Gateway& gateway, const QString& action, const QString& arguments);

    void UpdateReachability();

    QString deviceId;
    quint16 listenPort = 0;
    QString beaconName;
    bool bBeaconPairing = false;
    bool bUpnpEnabled = false;
    bool bRunning = false;

    QTimer* tickTimer = nullptr;
    QList<QNetworkInterface> interfaces;
    QString interfaceSignature;
    QUdpSocket* beaconSocket = nullptr;
    bool bBeaconsBound = false;
    QHash<QString, Discovered> discovered;
    QHash<QString, qint64> hostSeenMs;   ///< When the current host of a device last sent a beacon.
    QHash<QString, qint64> signalledMs;  ///< When beaconReceived last went out for a device.

    QUdpSocket* stunSocket = nullptr;
    QTimer* stunTimer = nullptr;
    QTimer* stunRecheckTimer = nullptr;
    quint64 stunGeneration = 0;
    int stunServerIndex = 0;
    int stunAttempts = 0;
    QHostAddress stunServer;
    quint16 stunServerPort = 0;
    QByteArray stunTransaction;
    QHostAddress detectedAddress;

    QNetworkAccessManager* http = nullptr;
    QUdpSocket* ssdpSocket = nullptr;
    QTimer* ssdpTimer = nullptr;
    QTimer* refreshTimer = nullptr; ///< Repeats the router search and mapping (renews the lease or retries).
    quint64 upnpGeneration = 0;
    UpnpState upnpState = UpnpState::Off;
    QList<QUrl> ssdpLocations;
    QList<Gateway> gateways;
    int gatewayIndex = 0;
    QHostAddress gatewayExternal;  ///< External address of the gateway being tried.
    bool bMapped = false;
    Gateway mapping;
    quint16 mappedPort = 0;
    QHostAddress externalAddress;

    Reachability reachability = Reachability::Unknown;
    QString publicEndpoint;
};
