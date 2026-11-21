//
// Created by Stalker7274 on 21.11.2026.
//

#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// @brief Device sync for the UI (Settings › Sync, the Sync tab of Downloads) and its tray notifications. QML singleton.
///
/// Backed by SyncSubsystem. AppInstance creates it at startup, so the notifications about conflicts and files moved
/// to the Recycle Bin work before any QML asks for it.
class SyncViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool running READ IsRunning NOTIFY runningChanged)
    Q_PROPERTY(QString deviceName READ GetDeviceName NOTIFY devicesChanged)
    /// @brief [{ id, name, platform, paired, connected, connecting, nearby, pairing, rejected, address, status }]:
    /// paired devices, then unpaired ones heard on the local network. `status` is one line about the connection.
    Q_PROPERTY(QVariantList devices READ GetDevices NOTIFY devicesChanged)
    Q_PROPERTY(int pairedCount READ GetPairedCount NOTIFY devicesChanged)

    Q_PROPERTY(bool pairingOpen READ IsPairingOpen NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingCode READ GetPairingCode NOTIFY pairingChanged)
    /// @brief When the pairing code stops working, ms since the epoch.
    Q_PROPERTY(double pairingExpiresMs READ GetPairingExpiresMs NOTIFY pairingChanged)
    /// @brief A pairWith() attempt is running.
    Q_PROPERTY(bool pairingWith READ IsPairingWith NOTIFY pairingChanged)
    /// @brief The device name or address pairWith() is busy with.
    Q_PROPERTY(QString pairingTarget READ GetPairingTarget NOTIFY pairingChanged)
    /// @brief How the last pairing ended: the device name on success, the reason otherwise; empty before the first.
    Q_PROPERTY(QString pairMessage READ GetPairMessage NOTIFY pairMessageChanged)
    Q_PROPERTY(bool pairSucceeded READ HasPairSucceeded NOTIFY pairMessageChanged)

    /// @brief [{ address, deviceName, connected, connecting, error }]
    Q_PROPERTY(QVariantList manualAddresses READ GetManualAddresses NOTIFY manualAddressesChanged)

    Q_PROPERTY(Reachability reachability READ GetReachability NOTIFY networkChanged)
    Q_PROPERTY(UpnpState upnpState READ GetUpnpState NOTIFY networkChanged)
    /// @brief Whether devices on the internet can connect here, one line.
    Q_PROPERTY(QString reachabilityText READ GetReachabilityText NOTIFY networkChanged)
    /// @brief The router mapping, one line; empty when it is off.
    Q_PROPERTY(QString upnpText READ GetUpnpText NOTIFY networkChanged)
    /// @brief "host:port" devices on the internet can connect to; empty unless Public or Mapped.
    Q_PROPERTY(QString publicEndpoint READ GetPublicEndpoint NOTIFY networkChanged)
    Q_PROPERTY(QStringList localEndpoints READ GetLocalEndpoints NOTIFY networkChanged)
    Q_PROPERTY(bool hearingBeacons READ IsHearingBeacons NOTIFY networkChanged)
    Q_PROPERTY(int listenPort READ GetListenPort NOTIFY networkChanged)
    Q_PROPERTY(QString listenError READ GetListenError NOTIFY networkChanged)

    /// @brief [{ id, type, title, detail, peerName, mineLabel, mineText, theirsLabel, theirsText, bothLabel }], oldest
    /// first. An empty label means the choice does not apply to that conflict.
    Q_PROPERTY(QVariantList conflicts READ GetConflicts NOTIFY conflictsChanged)
    Q_PROPERTY(int conflictCount READ GetConflictCount NOTIFY conflictsChanged)
    /// @brief [{ title, peerName, progress (0..1), sizeText }]
    Q_PROPERTY(QVariantList transfers READ GetTransfers NOTIFY progressChanged)
    /// @brief What sync is doing, one line.
    Q_PROPERTY(QString statusText READ GetStatusText NOTIFY progressChanged)
    /// @brief Files are being read or received.
    Q_PROPERTY(bool busy READ IsBusy NOTIFY progressChanged)
    /// @brief Whether files are synced: there is a library folder to place received files in.
    Q_PROPERTY(bool fileSyncAvailable READ IsFileSyncAvailable NOTIFY progressChanged)
    /// @brief The folder received files are placed in.
    Q_PROPERTY(QString placementRoot READ GetPlacementRoot NOTIFY progressChanged)

public:
    /// @brief Mirrors SyncSubsystem::Reachability.
    enum Reachability
    {
        Unknown,
        Public,
        Mapped,
        Private
    };
    Q_ENUM(Reachability)

    /// @brief Mirrors SyncSubsystem::UpnpState.
    enum UpnpState
    {
        UpnpOff,
        UpnpSearching,
        UpnpMapped,
        UpnpFailed
    };
    Q_ENUM(UpnpState)

    /// @brief Mirrors SyncSubsystem::Choice.
    enum Choice
    {
        KeepMine,
        KeepTheirs,
        KeepBoth
    };
    Q_ENUM(Choice)

    /// @brief The instance shared by C++ and QML.
    static SyncViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static SyncViewModel* create(QQmlEngine*, QJSEngine*);

    bool IsRunning() const;
    QString GetDeviceName() const;
    QVariantList GetDevices() const;
    int GetPairedCount() const;

    bool IsPairingOpen() const;
    QString GetPairingCode() const;
    double GetPairingExpiresMs() const;
    bool IsPairingWith() const;
    QString GetPairingTarget() const;
    QString GetPairMessage() const { return pairMessage; }
    bool HasPairSucceeded() const { return bPairSucceeded; }

    QVariantList GetManualAddresses() const;

    Reachability GetReachability() const;
    UpnpState GetUpnpState() const;
    QString GetReachabilityText() const;
    QString GetUpnpText() const;
    QString GetPublicEndpoint() const;
    QStringList GetLocalEndpoints() const;
    bool IsHearingBeacons() const;
    int GetListenPort() const;
    QString GetListenError() const;

    QVariantList GetConflicts() const;
    int GetConflictCount() const;
    QVariantList GetTransfers() const;
    QString GetStatusText() const;
    bool IsBusy() const;
    bool IsFileSyncAvailable() const;
    QString GetPlacementRoot() const;

    /// @brief Stores a new device name; empty means the computer name.
    Q_INVOKABLE void setDeviceName(const QString& name);

    /// @brief Shows a pairing code for 10 minutes.
    /// @return Why it could not open; empty when it did.
    Q_INVOKABLE QString openPairing();
    Q_INVOKABLE void closePairing();
    /// @brief Pairs with a device that shows a code; pairMessage tells the result.
    /// @param target A device id from `devices`, or an address ("host", "host:port", "[ipv6]:port").
    /// @return Why the attempt did not start; empty when it did.
    Q_INVOKABLE QString pairWith(const QString& target, const QString& code);
    Q_INVOKABLE void cancelPairWith();
    /// @brief Forgets a paired device; a connected one forgets this device too.
    Q_INVOKABLE void unpair(const QString& deviceId);

    /// @brief Adds an address of a paired device to try besides discovery.
    /// @return Why it was not added; empty when it was.
    Q_INVOKABLE QString addManualAddress(const QString& address);
    Q_INVOKABLE void removeManualAddress(const QString& address);

    /// @brief Resolves conflicts with one choice (Choice); those it does not apply to stay open.
    Q_INVOKABLE void resolve(const QStringList& ids, int choice);
    /// @brief Rescans the library folders and indexes them again.
    Q_INVOKABLE void reindex();
    /// @brief Opens the folder received files are placed in.
    Q_INVOKABLE void openPlacementFolder();

signals:
    void runningChanged();
    void devicesChanged();
    void pairingChanged();
    void pairMessageChanged();
    void manualAddressesChanged();
    void networkChanged();
    void conflictsChanged();
    void progressChanged();
    /// @brief The user asked to see the conflicts (a notification was clicked): the Downloads screen shows its Sync tab.
    void conflictsRequested();

private:
    SyncViewModel();

    void ShowConflicts();

    QString pairMessage;
    bool bPairSucceeded = false;
};
