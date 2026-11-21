//
// Created by Stalker7274 on 21.11.2026.
//

#include "SyncViewModel.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QJSEngine>
#include <QLocale>
#include <QProcess>
#include <QUrl>

#include "AppInstance.h"
#include "Navigation.h"
#include "Sync/SyncSubsystem.h"
#include "TrayIcon.h"

namespace
{
    QString SizeText(qint64 bytes)
    {
        return QLocale(QLocale::English).formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
    }

    QString Plural(qint64 count, const QString& one, const QString& many)
    {
        return count == 1 ? one : many.arg(count);
    }

    QString LastSeenText(qint64 lastSeenMs)
    {
        if (lastSeenMs <= 0)
        {
            return QStringLiteral("Not connected yet");
        }
        const qint64 seconds = (QDateTime::currentMSecsSinceEpoch() - lastSeenMs) / 1000;
        if (seconds < 60)
        {
            return QStringLiteral("Last seen just now");
        }
        if (seconds < 3600)
        {
            return QStringLiteral("Last seen %1 min ago").arg(seconds / 60);
        }
        if (seconds < 86400)
        {
            return QStringLiteral("Last seen %1 h ago").arg(seconds / 3600);
        }
        return QStringLiteral("Last seen ") +
               QLocale(QLocale::English).toString(QDateTime::fromMSecsSinceEpoch(lastSeenMs).date(), QStringLiteral("d MMM yyyy"));
    }

    QString DeviceStatus(const SyncSubsystem::DeviceInfo& device)
    {
        if (!device.bPaired)
        {
            return device.bPairing ? QStringLiteral("Shows a pairing code") : QStringLiteral("Nearby, not paired");
        }
        if (device.bConnected)
        {
            return device.address.isEmpty() ? QStringLiteral("Connected") : QStringLiteral("Connected · ") + device.address;
        }
        if (device.bRejected)
        {
            return QStringLiteral("It no longer knows this PC: pair again");
        }
        if (device.bConnecting)
        {
            return QStringLiteral("Connecting…");
        }
        if (device.bNearby)
        {
            return QStringLiteral("Nearby, not connected");
        }
        return LastSeenText(device.lastSeenMs);
    }

    TrayIcon* Tray()
    {
        AppInstance* app = AppInstance::getInstance();
        return app ? app->getTrayIcon() : nullptr;
    }
}

SyncViewModel::SyncViewModel()
{
    SyncSubsystem& sync = SyncSubsystem::Get();
    connect(&sync, &SyncSubsystem::runningChanged, this, [this]
    {
        emit runningChanged();
        emit devicesChanged();
        emit pairingChanged();
        emit manualAddressesChanged();
        emit networkChanged();
        emit conflictsChanged();
        emit progressChanged();
    });
    connect(&sync, &SyncSubsystem::devicesChanged, this, &SyncViewModel::devicesChanged);
    connect(&sync, &SyncSubsystem::devicesChanged, this, &SyncViewModel::manualAddressesChanged);
    connect(&sync, &SyncSubsystem::devicesChanged, this, &SyncViewModel::progressChanged);
    connect(&sync, &SyncSubsystem::pairingChanged, this, &SyncViewModel::pairingChanged);
    connect(&sync, &SyncSubsystem::pairingChanged, this, &SyncViewModel::devicesChanged);
    connect(&sync, &SyncSubsystem::manualAddressesChanged, this, &SyncViewModel::manualAddressesChanged);
    connect(&sync, &SyncSubsystem::networkChanged, this, &SyncViewModel::networkChanged);
    connect(&sync, &SyncSubsystem::progressChanged, this, &SyncViewModel::progressChanged);
    connect(&sync, &SyncSubsystem::conflictsChanged, this, &SyncViewModel::conflictsChanged);
    connect(&sync, &SyncSubsystem::conflictsChanged, this, &SyncViewModel::progressChanged);

    connect(&sync, &SyncSubsystem::pairFinished, this, [this](bool bSuccess, const QString& message)
    {
        pairMessage = bSuccess ? QStringLiteral("Paired with %1").arg(message) : message;
        bPairSucceeded = bSuccess;
        emit pairMessageChanged();
    });

    connect(&sync, &SyncSubsystem::conflictsAdded, this, [this](int count)
    {
        if (TrayIcon* tray = Tray())
        {
            tray->ShowMessage(Plural(count, QStringLiteral("A sync conflict"), QStringLiteral("%1 sync conflicts")),
                QStringLiteral("Choose which version to keep in Downloads › Sync"), [this] { ShowConflicts(); });
        }
    });

    connect(&sync, &SyncSubsystem::filesTrashed, this, [](int count, const QString& peerName)
    {
        if (TrayIcon* tray = Tray())
        {
            tray->ShowMessage(
                Plural(count, QStringLiteral("A file went to the Recycle Bin"), QStringLiteral("%1 files went to the Recycle Bin")),
                QStringLiteral("Deleted on %1. Click to open the Recycle Bin").arg(peerName),
                [] { QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("shell:RecycleBinFolder")}); });
        }
    });
}

SyncViewModel& SyncViewModel::Get()
{
    static SyncViewModel instance;
    return instance;
}

SyncViewModel* SyncViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

bool SyncViewModel::IsRunning() const
{
    return SyncSubsystem::Get().IsRunning();
}

QString SyncViewModel::GetDeviceName() const
{
    return SyncSubsystem::Get().DeviceName();
}

QVariantList SyncViewModel::GetDevices() const
{
    QVariantList result;
    for (const SyncSubsystem::DeviceInfo& device : SyncSubsystem::Get().GetDevices())
    {
        result.append(QVariantMap{
            {QStringLiteral("id"), device.id},
            {QStringLiteral("name"), device.name},
            {QStringLiteral("platform"), device.platform},
            {QStringLiteral("paired"), device.bPaired},
            {QStringLiteral("connected"), device.bConnected},
            {QStringLiteral("connecting"), device.bConnecting},
            {QStringLiteral("nearby"), device.bNearby},
            {QStringLiteral("pairing"), device.bPairing},
            {QStringLiteral("rejected"), device.bRejected},
            {QStringLiteral("address"), device.address},
            {QStringLiteral("status"), DeviceStatus(device)},
        });
    }
    return result;
}

int SyncViewModel::GetPairedCount() const
{
    int count = 0;
    for (const SyncSubsystem::DeviceInfo& device : SyncSubsystem::Get().GetDevices())
    {
        if (device.bPaired)
        {
            ++count;
        }
    }
    return count;
}

bool SyncViewModel::IsPairingOpen() const
{
    return SyncSubsystem::Get().IsPairingOpen();
}

QString SyncViewModel::GetPairingCode() const
{
    return SyncSubsystem::Get().PairingCode();
}

double SyncViewModel::GetPairingExpiresMs() const
{
    return static_cast<double>(SyncSubsystem::Get().PairingExpiresMs());
}

bool SyncViewModel::IsPairingWith() const
{
    return SyncSubsystem::Get().IsPairingWith();
}

QString SyncViewModel::GetPairingTarget() const
{
    const QString target = SyncSubsystem::Get().PairingTarget();
    for (const SyncSubsystem::DeviceInfo& device : SyncSubsystem::Get().GetDevices())
    {
        if (device.id == target)
        {
            return device.name;
        }
    }
    return target;
}

QVariantList SyncViewModel::GetManualAddresses() const
{
    QVariantList result;
    for (const SyncSubsystem::AddressInfo& address : SyncSubsystem::Get().GetManualAddresses())
    {
        result.append(QVariantMap{
            {QStringLiteral("address"), address.address},
            {QStringLiteral("deviceName"), address.deviceName},
            {QStringLiteral("connected"), address.bConnected},
            {QStringLiteral("connecting"), address.bConnecting},
            {QStringLiteral("error"), address.error},
        });
    }
    return result;
}

SyncViewModel::Reachability SyncViewModel::GetReachability() const
{
    return static_cast<Reachability>(SyncSubsystem::Get().GetReachability());
}

SyncViewModel::UpnpState SyncViewModel::GetUpnpState() const
{
    return static_cast<UpnpState>(SyncSubsystem::Get().GetUpnpState());
}

QString SyncViewModel::GetReachabilityText() const
{
    const SyncSubsystem& sync = SyncSubsystem::Get();
    if (!sync.IsRunning())
    {
        return QStringLiteral("Sync is off");
    }
    if (sync.ListenPort() == 0)
    {
        return QStringLiteral("The sync server isn't running: this PC only connects to other devices");
    }
    switch (sync.GetReachability())
    {
    case SyncSubsystem::Reachability::Public:
        return QStringLiteral("This PC has a public address and can be the server for devices anywhere: %1")
            .arg(sync.PublicEndpoint());
    case SyncSubsystem::Reachability::Mapped:
        return QStringLiteral("The router forwards %1 to this PC, so it can be the server for devices anywhere")
            .arg(sync.PublicEndpoint());
    case SyncSubsystem::Reachability::Private:
        return QStringLiteral("Behind a router without a port mapping: devices on the internet can't connect here, "
                              "but this PC still connects to devices that can be the server");
    case SyncSubsystem::Reachability::Unknown:
        break;
    }
    return QStringLiteral("Checking whether devices on the internet can connect here…");
}

QString SyncViewModel::GetUpnpText() const
{
    switch (SyncSubsystem::Get().GetUpnpState())
    {
    case SyncSubsystem::UpnpState::Searching:
        return QStringLiteral("Looking for the router…");
    case SyncSubsystem::UpnpState::Mapped:
        return QStringLiteral("The router forwards the port");
    case SyncSubsystem::UpnpState::Failed:
        return QStringLiteral("The router doesn't forward ports on request (UPnP is off or unsupported)");
    case SyncSubsystem::UpnpState::Off:
        break;
    }
    return {};
}

QString SyncViewModel::GetPublicEndpoint() const
{
    return SyncSubsystem::Get().PublicEndpoint();
}

QStringList SyncViewModel::GetLocalEndpoints() const
{
    return SyncSubsystem::Get().LocalEndpoints();
}

bool SyncViewModel::IsHearingBeacons() const
{
    return SyncSubsystem::Get().IsHearingBeacons();
}

int SyncViewModel::GetListenPort() const
{
    return SyncSubsystem::Get().ListenPort();
}

QString SyncViewModel::GetListenError() const
{
    return SyncSubsystem::Get().ListenError();
}

QVariantList SyncViewModel::GetConflicts() const
{
    QVariantList result;
    for (const SyncSubsystem::ConflictInfo& conflict : SyncSubsystem::Get().GetConflicts())
    {
        result.append(QVariantMap{
            {QStringLiteral("id"), conflict.id},
            {QStringLiteral("type"), conflict.type},
            {QStringLiteral("title"), conflict.title},
            {QStringLiteral("detail"), conflict.detail},
            {QStringLiteral("peerName"), conflict.peerName},
            {QStringLiteral("mineLabel"), conflict.mineLabel},
            {QStringLiteral("mineText"), conflict.mineText},
            {QStringLiteral("theirsLabel"), conflict.theirsLabel},
            {QStringLiteral("theirsText"), conflict.theirsText},
            {QStringLiteral("bothLabel"), conflict.bothLabel},
        });
    }
    return result;
}

int SyncViewModel::GetConflictCount() const
{
    return static_cast<int>(SyncSubsystem::Get().GetConflicts().size());
}

QVariantList SyncViewModel::GetTransfers() const
{
    QVariantList result;
    for (const SyncSubsystem::TransferInfo& transfer : SyncSubsystem::Get().GetTransfers())
    {
        const qreal progress = transfer.size > 0 ? static_cast<qreal>(transfer.done) / static_cast<qreal>(transfer.size) : 0.0;
        result.append(QVariantMap{
            {QStringLiteral("title"), transfer.title},
            {QStringLiteral("peerName"), transfer.peerName},
            {QStringLiteral("progress"), qBound<qreal>(0.0, progress, 1.0)},
            {QStringLiteral("sizeText"), SizeText(transfer.done) + QStringLiteral(" / ") + SizeText(transfer.size)},
        });
    }
    return result;
}

QString SyncViewModel::GetStatusText() const
{
    const SyncSubsystem& sync = SyncSubsystem::Get();
    if (!sync.IsRunning())
    {
        return sync.IsEnabled() ? QStringLiteral("Starting…") : QStringLiteral("Sync is off");
    }

    const SyncSubsystem::Status status = sync.GetStatus();
    QStringList parts;
    if (status.connected == 0)
    {
        parts << QStringLiteral("No paired device connected");
    }
    else
    {
        QString name;
        for (const SyncSubsystem::DeviceInfo& device : sync.GetDevices())
        {
            if (device.bConnected)
            {
                name = device.name;
                break;
            }
        }
        parts << (status.connected == 1 ? QStringLiteral("Connected to %1").arg(name)
                                        : QStringLiteral("Connected to %1 devices").arg(status.connected));
    }

    if (status.blocked == QStringLiteral("disk_full"))
    {
        parts << QStringLiteral("Waiting: the disk is full");
    }
    else if (!status.blocked.isEmpty())
    {
        parts << QStringLiteral("Waiting: %1").arg(status.blocked);
    }
    if (status.transfers > 0)
    {
        parts << Plural(status.transfers, QStringLiteral("Receiving a file"), QStringLiteral("Receiving %1 files")) +
                     QStringLiteral(" · ") + SizeText(status.transferDone) + QStringLiteral(" of ") + SizeText(status.transferBytes);
    }
    if (status.missingFiles > status.transfers)
    {
        const qint64 waiting = status.missingFiles - status.transfers;
        parts << Plural(waiting, QStringLiteral("1 file to receive"), QStringLiteral("%1 files to receive")) +
                     QStringLiteral(" (") + SizeText(status.missingBytes) + QStringLiteral(")");
    }
    if (status.hashing > 0)
    {
        parts << Plural(status.hashing, QStringLiteral("Indexing 1 file"), QStringLiteral("Indexing %1 files"));
    }
    if (status.deferred > 0)
    {
        parts << Plural(status.deferred, QStringLiteral("1 change waits for the player to leave its file"),
            QStringLiteral("%1 changes wait for the player to leave their files"));
    }
    if (status.conflicts > 0)
    {
        parts << Plural(status.conflicts, QStringLiteral("1 conflict"), QStringLiteral("%1 conflicts"));
    }
    if (parts.size() == 1 && status.connected > 0)
    {
        parts << QStringLiteral("Up to date");
    }
    return parts.join(QStringLiteral(" · "));
}

bool SyncViewModel::IsBusy() const
{
    const SyncSubsystem::Status status = SyncSubsystem::Get().GetStatus();
    return status.hashing > 0 || status.transfers > 0;
}

bool SyncViewModel::IsFileSyncAvailable() const
{
    return SyncSubsystem::Get().IsFileSyncAvailable();
}

QString SyncViewModel::GetPlacementRoot() const
{
    return QDir::toNativeSeparators(SyncSubsystem::Get().PlacementRoot());
}

void SyncViewModel::setDeviceName(const QString& name)
{
    SyncSubsystem::Get().SetDeviceName(name);
}

QString SyncViewModel::openPairing()
{
    return SyncSubsystem::Get().OpenPairing();
}

void SyncViewModel::closePairing()
{
    SyncSubsystem::Get().ClosePairing();
}

QString SyncViewModel::pairWith(const QString& target, const QString& code)
{
    const QString error = SyncSubsystem::Get().PairWith(target.trimmed(), code);
    if (error.isEmpty() && !pairMessage.isEmpty())
    {
        pairMessage.clear();
        bPairSucceeded = false;
        emit pairMessageChanged();
    }
    return error;
}

void SyncViewModel::cancelPairWith()
{
    SyncSubsystem::Get().CancelPairWith();
}

void SyncViewModel::unpair(const QString& deviceId)
{
    SyncSubsystem::Get().Unpair(deviceId);
}

QString SyncViewModel::addManualAddress(const QString& address)
{
    return SyncSubsystem::Get().AddManualAddress(address.trimmed());
}

void SyncViewModel::removeManualAddress(const QString& address)
{
    SyncSubsystem::Get().RemoveManualAddress(address);
}

void SyncViewModel::resolve(const QStringList& ids, int choice)
{
    if (ids.isEmpty() || choice < KeepMine || choice > KeepBoth)
    {
        return;
    }
    SyncSubsystem::Get().Resolve(ids, static_cast<SyncSubsystem::Choice>(choice));
}

void SyncViewModel::reindex()
{
    SyncSubsystem::Get().Reindex();
}

void SyncViewModel::openPlacementFolder()
{
    const QString folder = SyncSubsystem::Get().PlacementRoot();
    if (!folder.isEmpty())
    {
        QDir().mkpath(folder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }
}

void SyncViewModel::ShowConflicts()
{
    if (AppInstance* app = AppInstance::getInstance())
    {
        app->showMainWindow();
    }
    Navigation::Get().navigate(Navigation::Downloads);
    emit conflictsRequested();
}
