//
// Created by Stalker7274 on 21.11.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

class GrpcCall;
class GrpcServer;
class QTimer;
class SyncEngine;
class SyncNetwork;
class SyncSession;

namespace soundlink::sync::v1
{
    class Frame;
}

/// @brief Sync of music files, playlists, user tags and saved online tracks between paired devices.
///
/// Every device runs a sync server (gRPC over plaintext HTTP/2, AppConfigs::SettingsKeys::SyncPort or one of the
/// ten ports after it), announces itself on the local network with UDP beacons and keeps a session with each
/// paired device it can reach: at the address its beacons come from, at the endpoints it announced (public
/// address, router mapping, LAN, IPv6) and at addresses entered by hand. Either side may dial; when both do,
/// both keep the same session. Pairing: one device shows a 6-digit code, the other types it.
///
/// What is replicated, how conflicts arise and how files are placed is SyncEngine's business
/// (see core/private/subsystems/Sync/DESIGN.md); this subsystem owns the network around it.
/// Turned on with AppConfigs::SettingsKeys::SyncEnabled.
class SyncSubsystem : public Subsystem<SyncSubsystem>
{
    Q_OBJECT
    friend class Subsystem<SyncSubsystem>;

public:
    /// @brief Default TCP port of the sync server. LAN beacons use the same number over UDP.
    static constexpr quint16 DefaultPort = 47777;

    /// @brief A side of a conflict, see Resolve().
    enum class Choice
    {
        Mine,   ///< This device's version.
        Theirs, ///< The other device's version.
        Both    ///< Keep both: a renamed copy of a file, or a playlist with the tracks of both.
    };

    /// @brief Whether devices on the internet can open connections to this one.
    enum class Reachability
    {
        Unknown, ///< Not known yet, or sync is off.
        Public,  ///< This device has a public ("white") IPv4 address: it can be the server for devices anywhere.
        Mapped,  ///< The router forwards a public port here (UPnP): it can be the server too.
        Private  ///< Behind NAT without a mapping: the local network only, but it can still connect out.
    };

    /// @brief The router port mapping.
    enum class UpnpState
    {
        Off,
        Searching,
        Mapped,
        Failed
    };

    /// @brief A paired device, or an unpaired one heard on the local network.
    struct DeviceInfo
    {
        QString id;
        QString name;
        int platform = 0;          ///< 1 Windows, 2 Android, 0 unknown.
        bool bPaired = false;
        bool bConnected = false;
        bool bConnecting = false;  ///< A connection to it is being opened.
        bool bNearby = false;      ///< Its beacons are heard on the local network.
        bool bPairing = false;     ///< It shows a pairing code right now.
        bool bRejected = false;    ///< It answered that it is not paired with this device: pair again.
        QString address;           ///< Where it is connected, heard or was last announced.
        qint64 lastSeenMs = 0;     ///< Last time a session with it was open; 0 when never.
    };

    /// @brief An address entered by hand and what answered there.
    struct AddressInfo
    {
        QString address;     ///< "host:port" as stored.
        QString deviceName;  ///< The device that answered last; empty when none did yet.
        bool bConnected = false;
        bool bConnecting = false;
        QString error;       ///< Why the last attempt failed; empty when it did not.
    };

    /// @brief An open conflict, ready to show. An empty label means the choice does not apply.
    struct ConflictInfo
    {
        QString id;
        int type = 0;          ///< Sync::ConflictType.
        QString title;         ///< File name or playlist name.
        QString detail;        ///< What happened, one line.
        QString peerName;
        QString mineLabel;
        QString mineText;      ///< This device's version (size and date, track count...).
        QString theirsLabel;
        QString theirsText;
        QString bothLabel;
        qint64 createdMs = 0;
    };

    /// @brief A file being received.
    struct TransferInfo
    {
        QString title;
        QString peerName;  ///< Empty for a copy of a local file.
        qint64 size = 0;
        qint64 done = 0;
    };

    /// @brief What sync is busy with.
    struct Status
    {
        bool bRunning = false;
        int connected = 0;        ///< Paired devices with an open session.
        int hashing = 0;          ///< Local files whose content is still being read.
        int missingFiles = 0;     ///< Files this device should hold and does not yet.
        qint64 missingBytes = 0;
        int transfers = 0;
        qint64 transferBytes = 0;
        qint64 transferDone = 0;
        int deferred = 0;         ///< Replacements and deletions waiting for the player to leave a file.
        QString blocked;          ///< Why downloads wait ("disk_full"); empty when they do not.
        int conflicts = 0;
    };

    /// @brief Starts sync if the setting allows. Called once from AppInstance::init.
    void Start();
    /// @brief Applies a change of one of the sync settings (AppConfigs::SettingsKeys::Sync*).
    void ApplySetting(const QString& key);
    bool IsEnabled() const;
    bool IsRunning() const { return engine != nullptr; }

    /// @brief This device's id; empty until sync was started in this run.
    QString DeviceId() const;
    /// @brief The name other devices see: the setting, or the computer name.
    QString DeviceName() const;
    /// @brief Stores a new device name (empty: the computer name) and announces it.
    void SetDeviceName(const QString& name);
    /// @brief Port the sync server listens on; 0 when it does not.
    quint16 ListenPort() const { return listenPort; }
    /// @brief Why the sync server could not listen; empty when it does or sync is off.
    QString ListenError() const { return listenError; }

    /// @brief Paired devices, then unpaired devices heard on the local network.
    QList<DeviceInfo> GetDevices() const;

    /// @brief Shows a pairing code for 10 minutes; another device pairs by typing it.
    /// @return Why pairing could not open (sync is off, the server does not listen); empty when it did.
    QString OpenPairing();
    void ClosePairing();
    bool IsPairingOpen() const { return !pairingCode.isEmpty(); }
    /// @brief The 6 digits to type on the other device; empty when pairing is closed.
    QString PairingCode() const { return pairingCode; }
    /// @brief When the code stops working, ms since the epoch.
    qint64 PairingExpiresMs() const { return pairingExpiresMs; }

    /// @brief Pairs with a device that shows a code: pairFinished() tells the result.
    /// @param target The device id of a nearby device, or an address ("host", "host:port", "[ipv6]:port").
    /// @return Why the attempt did not start; empty when it did.
    QString PairWith(const QString& target, const QString& code);
    bool IsPairingWith() const { return !pairSession.isNull(); }
    /// @brief The device id or address PairWith() is busy with.
    QString PairingTarget() const { return pairTarget; }
    void CancelPairWith();
    /// @brief Forgets a paired device; a connected one forgets this device too.
    void Unpair(const QString& deviceId);

    QList<AddressInfo> GetManualAddresses() const;
    /// @brief Adds an address of a paired device to try besides discovery.
    /// @return Why it was not added; empty when it was.
    QString AddManualAddress(const QString& address);
    void RemoveManualAddress(const QString& address);

    Reachability GetReachability() const;
    UpnpState GetUpnpState() const;
    /// @brief "host:port" devices on the internet can connect to; empty unless Public or Mapped.
    QString PublicEndpoint() const;
    /// @brief The public IPv4 address a STUN server saw; empty when unknown.
    QString DetectedAddress() const;
    /// @brief "ip:port" of the sync server on the local networks.
    QStringList LocalEndpoints() const;
    /// @brief False when another program holds the beacon port: nearby devices are not heard.
    bool IsHearingBeacons() const;

    /// @brief Open conflicts, oldest first.
    QList<ConflictInfo> GetConflicts() const;
    /// @brief Resolves conflicts with one choice; those it does not apply to stay open.
    void Resolve(const QStringList& ids, Choice choice);
    QList<TransferInfo> GetTransfers() const;
    Status GetStatus() const;
    /// @brief Rescans the library folders and indexes them again.
    void Reindex();
    /// @brief Whether files are synced: there is a library folder to place received files in.
    bool IsFileSyncAvailable() const;
    /// @brief The folder received files are placed in.
    QString PlacementRoot() const;

signals:
    void runningChanged();
    /// @brief Paired or nearby devices, their connections or this device's name changed.
    void devicesChanged();
    /// @brief The pairing code, its state or a PairWith() attempt changed.
    void pairingChanged();
    /// @brief A pairing ended: a PairWith() attempt, or another device typed this device's code (only successes).
    /// `message` is the device name on success, the reason otherwise.
    void pairFinished(bool bSuccess, const QString& message);
    void manualAddressesChanged();
    /// @brief Reachability, the router mapping, the endpoints or the server port changed.
    void networkChanged();
    void progressChanged();
    void conflictsChanged();
    /// @brief New conflicts appeared.
    void conflictsAdded(int count);
    /// @brief Local files went to the Recycle Bin because a paired device deleted them.
    void filesTrashed(int count, const QString& peerName);

private:
    /// Connection state of one paired device.
    struct PeerLink
    {
        QPointer<SyncSession> active;   ///< Established and attached to the engine.
        QPointer<SyncSession> dialing;  ///< Outgoing handshake in progress.
        QStringList round;              ///< Addresses of the running dial round.
        int candidate = 0;              ///< Next address of the round.
        qint64 backoffMs = 0;
        qint64 nextDialMs = 0;
        qint64 beaconDialMs = 0;        ///< When a beacon last started a dial.
        bool bRejected = false;         ///< It answered "not_paired".
    };

    /// An address entered by hand.
    struct ManualTarget
    {
        QString address;
        QString host;
        quint16 port = 0;
        QPointer<SyncSession> dialing;
        QString peerId;    ///< The device that answered last.
        QString peerName;
        QString error;
        qint64 backoffMs = 0;
        qint64 nextDialMs = 0;
    };

    /// What this subsystem knows about one of its sessions.
    struct SessionInfo
    {
        QString address;        ///< Outgoing: "host:port" it was dialed at.
        QString peerId;         ///< Outgoing to a paired device: who must answer.
        QString manualAddress;  ///< Dialed for this manual address.
        qint64 establishedMs = 0;
        bool bPair = false;     ///< The client side of PairWith().
    };

    SyncSubsystem();

    void Deinitialize() override;

    void StartSync();
    void StopSync(bool bWaitForRouter);

    void OnIncomingCall(GrpcCall* call);
    /// Client side when `call` is a client call, server side otherwise.
    SyncSession* CreateSession(GrpcCall* call, bool bPair, const QString& code, const QString& expectedPeerId);
    SyncSession* Dial(const QString& host, quint16 port, bool bPair, const QString& code, const QString& expectedPeerId);
    void Watch(SyncSession* session, const SessionInfo& info);
    void OnEstablished(SyncSession* session);
    void OnClosed(SyncSession* session, int status, const QString& reason);
    void OnFrame(SyncSession* session, const soundlink::sync::v1::Frame& frame);
    /// Whether `fresh` replaces `current` as the session with the same device.
    bool Replaces(SyncSession* fresh, SyncSession* current) const;
    void SendEndpoints(SyncSession* session);
    void OnNetworkChanged();

    void DialTick();
    void OnBeacon(const QString& peerId);
    /// The network changed: disconnected devices and manual addresses are tried again soon.
    void ResetDialBackoff();
    void DialPeer(const QString& peerId);
    QStringList CandidatesFor(const QString& peerId) const;
    void DialManual(ManualTarget& target);
    bool IsBusyWith(const QString& peerId) const;
    void RebuildManualTargets();
    void SyncLinksWithPeers();

    QString NewPairingCode() const;
    void OnBadCode();

    SyncEngine* engine = nullptr;
    SyncNetwork* network = nullptr;
    GrpcServer* server = nullptr;
    QTimer* dialTimer = nullptr;
    QTimer* pairingTimer = nullptr;
    quint16 listenPort = 0;
    QString listenError;
    QString deviceId;
    bool bStarted = false;

    QHash<SyncSession*, SessionInfo> sessions;
    QHash<QString, PeerLink> links;
    QList<ManualTarget> manualTargets;
    QStringList sentEndpoints;

    QString pairingCode;
    qint64 pairingExpiresMs = 0;
    int badCodes = 0;
    int codeRegenerations = 0;

    QPointer<SyncSession> pairSession;
    QString pairTarget;
};
