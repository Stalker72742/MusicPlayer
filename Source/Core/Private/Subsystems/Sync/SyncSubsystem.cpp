//
// Created by Stalker7274 on 21.11.2026.
//

#include "Sync/SyncSubsystem.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#include <QTomlUtils/QTomlUtils.h>

#include <algorithm>
#include <optional>

#include "AppConfigs.h"
#include "GrpcTransport.h"
#include "SyncData.h"
#include "SyncEngine.h"
#include "SyncNetwork.h"
#include "SyncSession.h"

namespace proto = soundlink::sync::v1;
namespace Keys = AppConfigs::SettingsKeys;

namespace
{
    const QByteArray SessionPath = QByteArrayLiteral("/soundlink.sync.v1.SyncService/Session");

    constexpr int PortAttempts = 11;
    constexpr int DialTickMs = 1000;
    constexpr int MaxPendingIncoming = 16;
    constexpr int RefusedCallTimeoutMs = 10000;
    constexpr qint64 CandidateGapMs = 500;
    constexpr qint64 MinBackoffMs = 5000;
    constexpr qint64 MaxBackoffMs = 300000;
    constexpr qint64 RejectedBackoffMs = 300000;
    constexpr qint64 BeaconDialGapMs = 10000;
    constexpr qint64 DuplicateWindowMs = 10000;
    constexpr qint64 PairingLifetimeMs = 600000;
    constexpr int MaxBadCodes = 5;
    constexpr int MaxCodeRegenerations = 3;
    constexpr int MaxPeerAddresses = 16;
    constexpr int MaxManualAddresses = 32;
    constexpr int MaxDeviceNameLength = 64;
    constexpr int MaxAddressLength = 300;
    constexpr int PlatformWindows = 1;

    qint64 NowMs()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }

    qint64 Jitter(int fromMs, int toMs)
    {
        return QRandomGenerator::global()->bounded(fromMs, toMs);
    }

    qint64 NextBackoff(qint64 current)
    {
        return current <= 0 ? MinBackoffMs : std::min(current * 2, MaxBackoffMs);
    }

    bool IsRejection(const QString& reason)
    {
        return reason == QLatin1String("not_paired") || reason == QLatin1String("bad_key");
    }

    struct HostPort
    {
        QString host;
        quint16 port = 0;
    };

    std::optional<HostPort> ParseAddress(const QString& text, quint16 defaultPort)
    {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty() || trimmed.size() > MaxAddressLength)
        {
            return std::nullopt;
        }

        QString host;
        QString portText;
        bool bHasPort = false;
        if (trimmed.startsWith(u'['))
        {
            const qsizetype close = trimmed.indexOf(u']');
            if (close < 0)
            {
                return std::nullopt;
            }
            host = trimmed.mid(1, close - 1);
            const QString rest = trimmed.mid(close + 1);
            if (!rest.isEmpty())
            {
                if (!rest.startsWith(u':'))
                {
                    return std::nullopt;
                }
                portText = rest.mid(1);
                bHasPort = true;
            }
            const QHostAddress address(host);
            if (address.protocol() != QAbstractSocket::IPv6Protocol)
            {
                return std::nullopt;
            }
        }
        else if (trimmed.count(u':') > 1)
        {
            host = trimmed;
        }
        else
        {
            const qsizetype colon = trimmed.indexOf(u':');
            if (colon >= 0)
            {
                host = trimmed.left(colon);
                portText = trimmed.mid(colon + 1);
                bHasPort = true;
            }
            else
            {
                host = trimmed;
            }
        }

        quint16 port = defaultPort;
        if (bHasPort)
        {
            bool bOk = false;
            const uint value = portText.toUInt(&bOk);
            if (!bOk || value == 0 || value > 65535)
            {
                return std::nullopt;
            }
            port = static_cast<quint16>(value);
        }
        if (port == 0 || host.isEmpty())
        {
            return std::nullopt;
        }

        const QHostAddress address(host);
        if (address.protocol() == QAbstractSocket::IPv4Protocol || address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            return HostPort{address.toString(), port};
        }

        static const QRegularExpression HostPattern(QStringLiteral(
            "^(?=.{1,253}\\.?$)[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?(?:\\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*\\.?$"));
        if (!HostPattern.match(host).hasMatch())
        {
            return std::nullopt;
        }
        host = host.toLower();
        if (host.endsWith(u'.'))
        {
            host.chop(1);
        }
        return HostPort{host, port};
    }

    QString FormatAddress(const QString& host, quint16 port)
    {
        return host.contains(u':') ? QStringLiteral("[%1]:%2").arg(host).arg(port) : QStringLiteral("%1:%2").arg(host).arg(port);
    }

    bool SameAddress(const HostPort& a, const HostPort& b)
    {
        return a.port == b.port && a.host.compare(b.host, Qt::CaseInsensitive) == 0;
    }

    QString FileText(const Sync::Record& record)
    {
        if (record.bDeleted)
        {
            return QStringLiteral("Deleted");
        }
        const QLocale english(QLocale::English);
        QString text = english.formattedDataSize(static_cast<qint64>(record.file.size), 1, QLocale::DataSizeTraditionalFormat);
        if (record.file.mtimeMs > 0)
        {
            text += QStringLiteral(" · ") +
                    english.toString(QDateTime::fromMSecsSinceEpoch(record.file.mtimeMs), QStringLiteral("d MMM yyyy, HH:mm"));
        }
        return text;
    }

    QString FileTitle(const Sync::Record& record)
    {
        const QString name = QFileInfo(record.file.relPath).fileName();
        if (!name.isEmpty())
        {
            return name;
        }
        const Sync::TrackInfo& info = record.file.info;
        if (!info.title.isEmpty())
        {
            return info.artist.isEmpty() ? info.title : info.artist + QStringLiteral(" - ") + info.title;
        }
        return QStringLiteral("Untitled file");
    }

    QString TrackCount(qsizetype count)
    {
        return count == 1 ? QStringLiteral("1 track") : QStringLiteral("%1 tracks").arg(count);
    }

    QString PlaylistText(const Sync::Record& record, bool bWithName)
    {
        if (record.bDeleted)
        {
            return QStringLiteral("Deleted");
        }
        QString text = record.playlist.bSmart ? QStringLiteral("Smart playlist") : TrackCount(record.playlist.tracks.size());
        if (bWithName)
        {
            text = record.playlist.name + QStringLiteral(" · ") + text;
        }
        return text;
    }

    SyncSubsystem::ConflictInfo Describe(const Sync::Conflict& conflict, const Sync::State& state)
    {
        SyncSubsystem::ConflictInfo info;
        info.id = conflict.id;
        info.type = static_cast<int>(conflict.type);
        info.peerName = conflict.peerName;
        info.createdMs = conflict.createdMs;

        const QString peer = conflict.peerName.isEmpty() ? QStringLiteral("another device") : conflict.peerName;
        const Sync::Record mine = conflict.mine.uid.isEmpty() ? state.entries.value(conflict.key).record : conflict.mine;
        const Sync::Record& theirs = conflict.theirs;

        switch (conflict.type)
        {
        case Sync::ConflictType::Content:
            info.title = FileTitle(mine);
            info.detail = QStringLiteral("Changed on this PC and on %1").arg(peer);
            info.mineLabel = QStringLiteral("Keep this PC's");
            info.mineText = FileText(mine);
            info.theirsLabel = QStringLiteral("Take %1's").arg(peer);
            info.theirsText = FileText(theirs);
            info.bothLabel = QStringLiteral("Keep both");
            break;
        case Sync::ConflictType::DeleteVsEdit:
            info.title = FileTitle(mine.bDeleted ? theirs : mine);
            if (mine.bDeleted)
            {
                info.detail = QStringLiteral("Deleted on this PC, changed on %1").arg(peer);
                info.mineLabel = QStringLiteral("Delete");
                info.theirsLabel = QStringLiteral("Keep %1's").arg(peer);
            }
            else
            {
                info.detail = QStringLiteral("Deleted on %1, changed on this PC").arg(peer);
                info.mineLabel = QStringLiteral("Keep this PC's");
                info.theirsLabel = QStringLiteral("Delete");
            }
            info.mineText = FileText(mine);
            info.theirsText = FileText(theirs);
            break;
        case Sync::ConflictType::Name:
            info.title = !conflict.localPath.isEmpty() ? QFileInfo(conflict.localPath).fileName() : FileTitle(mine);
            info.detail = QStringLiteral("%1 has a different file with this name").arg(peer);
            info.mineLabel = QStringLiteral("Keep only this PC's");
            info.mineText = FileText(mine);
            info.theirsLabel = QStringLiteral("Keep only %1's").arg(peer);
            info.theirsText = FileText(theirs);
            info.bothLabel = QStringLiteral("Keep both");
            break;
        case Sync::ConflictType::Playlist:
        {
            const Sync::Record& alive = mine.bDeleted ? theirs : mine;
            info.title = alive.playlist.name.isEmpty() ? QStringLiteral("Untitled playlist") : alive.playlist.name;
            const bool bNamesDiffer = !mine.bDeleted && !theirs.bDeleted && mine.playlist.name != theirs.playlist.name;
            if (mine.bDeleted == theirs.bDeleted)
            {
                info.detail = QStringLiteral("Changed on this PC and on %1").arg(peer);
                info.mineLabel = QStringLiteral("Keep this PC's");
                info.theirsLabel = QStringLiteral("Take %1's").arg(peer);
                if (!mine.playlist.bSmart && !theirs.playlist.bSmart)
                {
                    info.bothLabel = QStringLiteral("Merge");
                }
            }
            else if (mine.bDeleted)
            {
                info.detail = QStringLiteral("Deleted on this PC, changed on %1").arg(peer);
                info.mineLabel = QStringLiteral("Delete");
                info.theirsLabel = QStringLiteral("Keep %1's").arg(peer);
            }
            else
            {
                info.detail = QStringLiteral("Deleted on %1, changed on this PC").arg(peer);
                info.mineLabel = QStringLiteral("Keep this PC's");
                info.theirsLabel = QStringLiteral("Delete");
            }
            info.mineText = PlaylistText(mine, bNamesDiffer);
            info.theirsText = PlaylistText(theirs, bNamesDiffer);
            break;
        }
        case Sync::ConflictType::RemoteChange:
            info.title = FileTitle(mine);
            if (theirs.bDeleted)
            {
                info.detail = QStringLiteral("%1 deleted this file").arg(peer);
                info.mineLabel = QStringLiteral("Keep it");
                info.theirsLabel = QStringLiteral("Delete");
            }
            else
            {
                info.detail = QStringLiteral("%1 replaced this file").arg(peer);
                info.mineLabel = QStringLiteral("Keep mine");
                info.theirsLabel = QStringLiteral("Take %1's").arg(peer);
            }
            info.mineText = FileText(mine);
            info.theirsText = FileText(theirs);
            break;
        case Sync::ConflictType::LocalDelete:
            info.title = !conflict.localPath.isEmpty() ? QFileInfo(conflict.localPath).fileName() : FileTitle(mine);
            info.detail = QStringLiteral("Disappeared from this PC along with many other files");
            info.mineLabel = QStringLiteral("Delete everywhere");
            info.mineText = QStringLiteral("Deleted here");
            info.theirsLabel = QStringLiteral("Restore");
            info.theirsText = FileText(theirs);
            break;
        }
        return info;
    }

    QString PairFailureText(int status, const QString& reason)
    {
        if (reason == QLatin1String("bad_code"))
        {
            return QStringLiteral("Wrong code");
        }
        if (reason == QLatin1String("pairing_closed"))
        {
            return QStringLiteral("Pairing isn't open on that device");
        }
        if (reason == QLatin1String("wrong_peer"))
        {
            return QStringLiteral("A different device answered");
        }
        if (reason == QLatin1String("busy"))
        {
            return QStringLiteral("The device is busy, try again");
        }
        if (reason == QLatin1String("protocol"))
        {
            return QStringLiteral("The device runs an incompatible version");
        }
        if (reason == QLatin1String("timeout") || status == GrpcStatus::Unavailable)
        {
            return QStringLiteral("Couldn't reach the device");
        }
        return reason.isEmpty() ? QStringLiteral("Pairing failed") : QStringLiteral("Pairing failed (%1)").arg(reason);
    }

    QString ConnectFailureText(int status, const QString& reason)
    {
        if (IsRejection(reason))
        {
            return QStringLiteral("Not paired with this device");
        }
        if (reason == QLatin1String("timeout"))
        {
            return QStringLiteral("No answer");
        }
        if (reason == QLatin1String("protocol"))
        {
            return QStringLiteral("Incompatible version");
        }
        if (reason == QLatin1String("busy"))
        {
            return QStringLiteral("Busy");
        }
        if (reason == QLatin1String("crypto"))
        {
            return QStringLiteral("Handshake failed");
        }
        if (reason.isEmpty())
        {
            return status == GrpcStatus::Unavailable ? QStringLiteral("Couldn't connect") : QStringLiteral("Connection failed");
        }
        return reason;
    }
}

SyncSubsystem::SyncSubsystem()
{
    pairingTimer = new QTimer(this);
    pairingTimer->setSingleShot(true);
    connect(pairingTimer, &QTimer::timeout, this, &SyncSubsystem::ClosePairing);

    dialTimer = new QTimer(this);
    dialTimer->setInterval(DialTickMs);
    connect(dialTimer, &QTimer::timeout, this, &SyncSubsystem::DialTick);
}

void SyncSubsystem::Start()
{
    if (bStarted)
    {
        return;
    }
    bStarted = true;
    RebuildManualTargets();
    if (IsEnabled())
    {
        StartSync();
    }
}

void SyncSubsystem::Deinitialize()
{
    StopSync(true);
    bStarted = false;
}

bool SyncSubsystem::IsEnabled() const
{
    return AppConfigs::GetBool(Keys::SyncEnabled);
}

void SyncSubsystem::StartSync()
{
    if (engine)
    {
        return;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/sync");
    QDir().mkpath(folder);

    engine = new SyncEngine(this);
    engine->Load(folder + QStringLiteral("/state.bin"));
    deviceId = engine->DeviceId();
    engine->SetConfirmRemote(AppConfigs::GetBool(Keys::SyncConfirmRemote));
    connect(engine, &SyncEngine::peersChanged, this, [this]
    {
        SyncLinksWithPeers();
        emit devicesChanged();
    });
    connect(engine, &SyncEngine::progressChanged, this, &SyncSubsystem::progressChanged);
    connect(engine, &SyncEngine::conflictsChanged, this, &SyncSubsystem::conflictsChanged);
    connect(engine, &SyncEngine::conflictsAdded, this, &SyncSubsystem::conflictsAdded);
    connect(engine, &SyncEngine::filesTrashed, this, &SyncSubsystem::filesTrashed);

    server = new GrpcServer(SessionPath, this);
    connect(server, &GrpcServer::incomingCall, this, &SyncSubsystem::OnIncomingCall);
    const int configured = QTomlUtils::FindPropertyValue<int>(AppConfigs::Settings, Keys::SyncPort, static_cast<int>(DefaultPort));
    const int basePort = configured >= 1 && configured <= 65535 ? configured : DefaultPort;
    QString firstError;
    for (int attempt = 0; attempt < PortAttempts && basePort + attempt <= 65535; ++attempt)
    {
        if (server->Listen(static_cast<quint16>(basePort + attempt)))
        {
            listenPort = server->Port();
            break;
        }
        if (firstError.isEmpty())
        {
            firstError = server->ErrorString();
        }
    }
    if (listenPort == 0)
    {
        listenError = firstError.isEmpty() ? QStringLiteral("No free port") : firstError;
        qWarning() << "Sync: the server could not listen:" << listenError;
    }

    network = new SyncNetwork(this);
    connect(network, &SyncNetwork::beaconReceived, this, [this](const SyncNetwork::Discovered& device)
    {
        OnBeacon(device.deviceId);
    });
    connect(network, &SyncNetwork::discoveredChanged, this, &SyncSubsystem::devicesChanged);
    connect(network, &SyncNetwork::networkChanged, this, &SyncSubsystem::OnNetworkChanged);
    network->SetBeacon(DeviceName(), false);
    network->Start(deviceId, listenPort, listenPort != 0 && AppConfigs::GetBool(Keys::SyncUpnp, true));

    engine->Start();
    sentEndpoints.clear();
    RebuildManualTargets();
    dialTimer->start();
    qInfo() << "Sync: started, device" << deviceId << "port" << listenPort;

    emit runningChanged();
    emit devicesChanged();
    emit networkChanged();
    emit progressChanged();
    emit conflictsChanged();
}

void SyncSubsystem::StopSync(bool bWaitForRouter)
{
    if (!engine)
    {
        return;
    }
    dialTimer->stop();
    disconnect(engine, nullptr, this, nullptr);
    disconnect(server, nullptr, this, nullptr);
    disconnect(network, nullptr, this, nullptr);

    pairingTimer->stop();
    pairingCode.clear();
    pairingExpiresMs = 0;
    badCodes = 0;
    codeRegenerations = 0;
    const bool bWasPairingWith = !pairTarget.isEmpty();
    pairSession.clear();
    pairTarget.clear();

    engine->Stop();
    const QList<SyncSession*> all = sessions.keys();
    sessions.clear();
    links.clear();
    for (SyncSession* session : all)
    {
        disconnect(session, nullptr, this, nullptr);
        session->Close(QStringLiteral("shutdown"));
        delete session;
    }
    for (ManualTarget& target : manualTargets)
    {
        target.dialing.clear();
        target.error.clear();
        target.backoffMs = 0;
        target.nextDialMs = 0;
    }

    server->Close();
    delete server;
    server = nullptr;
    network->Stop(bWaitForRouter);
    delete network;
    network = nullptr;
    delete engine;
    engine = nullptr;
    listenPort = 0;
    listenError.clear();
    sentEndpoints.clear();
    qInfo() << "Sync: stopped";

    if (bWasPairingWith)
    {
        emit pairFinished(false, QStringLiteral("Sync was turned off"));
    }
    emit runningChanged();
    emit devicesChanged();
    emit pairingChanged();
    emit manualAddressesChanged();
    emit networkChanged();
    emit progressChanged();
    emit conflictsChanged();
}

void SyncSubsystem::ApplySetting(const QString& key)
{
    if (key == Keys::SyncEnabled)
    {
        if (!bStarted)
        {
            return;
        }
        if (IsEnabled())
        {
            StartSync();
        }
        else
        {
            StopSync(false);
        }
    }
    else if (key == Keys::SyncDeviceName)
    {
        if (network)
        {
            network->SetBeacon(DeviceName(), IsPairingOpen());
        }
        emit devicesChanged();
    }
    else if (key == Keys::SyncPort)
    {
        if (engine)
        {
            StopSync(false);
            StartSync();
        }
    }
    else if (key == Keys::SyncUpnp)
    {
        if (network && listenPort != 0)
        {
            network->SetUpnp(AppConfigs::GetBool(Keys::SyncUpnp, true));
        }
        emit networkChanged();
    }
    else if (key == Keys::SyncConfirmRemote)
    {
        if (engine)
        {
            engine->SetConfirmRemote(AppConfigs::GetBool(Keys::SyncConfirmRemote));
        }
    }
    else if (key == Keys::SyncManualAddresses)
    {
        RebuildManualTargets();
    }
}

QString SyncSubsystem::DeviceId() const
{
    return engine ? engine->DeviceId() : deviceId;
}

QString SyncSubsystem::DeviceName() const
{
    const QString configured = AppConfigs::GetString(Keys::SyncDeviceName).simplified().left(MaxDeviceNameLength);
    if (!configured.isEmpty())
    {
        return configured;
    }
    const QString host = QSysInfo::machineHostName().simplified().left(MaxDeviceNameLength);
    return host.isEmpty() ? QStringLiteral("SoundLink") : host;
}

void SyncSubsystem::SetDeviceName(const QString& name)
{
    const QString clean = name.simplified().left(MaxDeviceNameLength);
    if (clean == AppConfigs::GetString(Keys::SyncDeviceName))
    {
        return;
    }
    QTomlUtils::SetPropertyValue(AppConfigs::Settings, Keys::SyncDeviceName, clean);
    ApplySetting(Keys::SyncDeviceName);
}

SyncSession* SyncSubsystem::CreateSession(GrpcCall* call, bool bPair, const QString& code, const QString& expectedPeerId)
{
    SyncSession::Identity identity;
    identity.deviceId = deviceId;
    identity.deviceName = DeviceName();
    identity.platform = PlatformWindows;
    identity.listenPort = listenPort;

    SyncSession::Hooks hooks;
    hooks.pskFor = [this](const QString& peerId) -> QByteArray
    {
        return engine ? engine->GetState().peers.value(peerId).psk : QByteArray();
    };
    hooks.pairingCode = [this]() -> QString
    {
        return !pairingCode.isEmpty() && NowMs() < pairingExpiresMs ? pairingCode : QString();
    };
    hooks.badCode = [this]
    {
        OnBadCode();
    };
    SyncSession::WelcomeProvider welcome = [this](const QString& peerId) -> proto::Welcome
    {
        return engine ? engine->MakeWelcome(peerId, DeviceName()) : proto::Welcome();
    };

    if (call->IsClient())
    {
        return SyncSession::Open(call, identity, bPair ? SyncSession::Mode::Pair : SyncSession::Mode::Sync, code,
                                 expectedPeerId, std::move(hooks), std::move(welcome), this);
    }
    return SyncSession::Accept(call, identity, std::move(hooks), std::move(welcome), this);
}

void SyncSubsystem::OnIncomingCall(GrpcCall* call)
{
    if (call->IsFinished())
    {
        call->deleteLater();
        return;
    }
    int pending = 0;
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        const SyncSession* session = it.key();
        if (!session->IsClient() && !session->IsEstablished() && !session->IsClosed())
        {
            ++pending;
        }
    }
    if (!engine || pending >= MaxPendingIncoming)
    {
        call->setParent(nullptr);
        connect(call, &GrpcCall::finished, call, &QObject::deleteLater);
        QTimer::singleShot(RefusedCallTimeoutMs, call, &GrpcCall::Abort);
        call->Finish(GrpcStatus::ResourceExhausted, QStringLiteral("busy"));
        return;
    }
    Watch(CreateSession(call, false, {}, {}), {});
}

SyncSession* SyncSubsystem::Dial(const QString& host, quint16 port, bool bPair, const QString& code, const QString& expectedPeerId)
{
    return CreateSession(GrpcCall::Connect(host, port, SessionPath), bPair, code, expectedPeerId);
}

void SyncSubsystem::Watch(SyncSession* session, const SessionInfo& info)
{
    sessions.insert(session, info);
    connect(session, &SyncSession::established, this, [this, session]
    {
        OnEstablished(session);
    });
    connect(session, &SyncSession::frameReceived, this, [this, session](const proto::Frame& frame)
    {
        OnFrame(session, frame);
    });
    connect(session, &SyncSession::closed, this, [this, session](int status, const QString& reason)
    {
        OnClosed(session, status, reason);
    });
}

void SyncSubsystem::OnEstablished(SyncSession* session)
{
    const auto infoIt = sessions.find(session);
    if (infoIt == sessions.end() || !engine)
    {
        return;
    }
    infoIt->establishedMs = NowMs();
    const SessionInfo info = *infoIt;
    const QString peerId = session->PeerId();

    if (session->GetMode() == SyncSession::Mode::Pair)
    {
        Sync::Peer peer;
        peer.id = peerId;
        peer.name = session->PeerName();
        peer.platform = session->PeerPlatform();
        peer.psk = session->NewPsk();
        peer.lastSeenMs = NowMs();
        engine->UpsertPeer(peer);
        if (engine->GetState().peers.contains(peerId))
        {
            PeerLink& link = links[peerId];
            link.bRejected = false;
            link.backoffMs = 0;
            link.nextDialMs = 0;
        }
        if (!session->IsClient())
        {
            ClosePairing();
        }
        else if (pairSession == session)
        {
            pairSession.clear();
            pairTarget.clear();
        }
        const QString name = peer.name.isEmpty() ? peerId.left(8) : peer.name;
        qInfo() << "Sync: paired with" << name << peerId;
        emit pairFinished(true, name);
        emit pairingChanged();
    }

    if (!engine->GetState().peers.contains(peerId))
    {
        session->Close(QStringLiteral("not_paired"));
        return;
    }

    if (!info.manualAddress.isEmpty())
    {
        for (ManualTarget& target : manualTargets)
        {
            if (target.address != info.manualAddress)
            {
                continue;
            }
            target.peerId = peerId;
            target.peerName = session->PeerName();
            target.error.clear();
            target.backoffMs = 0;
            target.nextDialMs = 0;
            if (target.dialing == session)
            {
                target.dialing.clear();
            }
        }
        emit manualAddressesChanged();
    }

    PeerLink& link = links[peerId];
    if (link.dialing == session)
    {
        link.dialing.clear();
    }
    SyncSession* replaced = nullptr;
    if (link.active && link.active != session && !link.active->IsClosed())
    {
        if (!Replaces(session, link.active))
        {
            session->Close(QStringLiteral("duplicate"));
            return;
        }
        replaced = link.active;
    }
    link.active = session;
    link.round.clear();
    link.candidate = 0;
    link.backoffMs = 0;
    link.nextDialMs = 0;
    link.bRejected = false;

    engine->AttachSession(session);
    if (replaced)
    {
        replaced->Close(QStringLiteral("duplicate"));
    }

    if (session->IsClient() && !info.address.isEmpty())
    {
        QStringList addresses = engine->GetState().peers.value(peerId).addresses;
        addresses.removeAll(info.address);
        addresses.prepend(info.address);
        engine->SetPeerAddresses(peerId, addresses.mid(0, MaxPeerAddresses));
    }

    SendEndpoints(session);
    if (session->IsClient())
    {
        qInfo() << "Sync: connected to" << session->PeerName() << peerId << "at" << info.address;
    }
    else
    {
        qInfo() << "Sync: connected to" << session->PeerName() << peerId << "from" << session->PeerHost();
    }
    emit devicesChanged();
}

bool SyncSubsystem::Replaces(SyncSession* fresh, SyncSession* current) const
{
    const QString freshClient = fresh->IsClient() ? deviceId : fresh->PeerId();
    const QString currentClient = current->IsClient() ? deviceId : current->PeerId();
    if (freshClient == currentClient)
    {
        return true;
    }
    if (NowMs() - sessions.value(current).establishedMs < DuplicateWindowMs)
    {
        return freshClient < currentClient;
    }
    return true;
}

void SyncSubsystem::OnClosed(SyncSession* session, int status, const QString& reason)
{
    const auto it = sessions.find(session);
    if (it == sessions.end())
    {
        session->deleteLater();
        return;
    }
    const SessionInfo info = *it;
    sessions.erase(it);
    session->deleteLater();

    const bool bEstablished = info.establishedMs != 0;
    const qint64 now = NowMs();
    const QString peerId = !info.peerId.isEmpty() ? info.peerId : session->PeerId();

    if (bEstablished)
    {
        qInfo() << "Sync: session with" << session->PeerName() << peerId << "closed:" << status << reason;
    }
    else if (session->IsClient())
    {
        qDebug() << "Sync: connection to" << info.address << "failed:" << status << reason;
    }
    else
    {
        qDebug() << "Sync: incoming connection from" << session->PeerHost() << "failed:" << status << reason;
    }

    if (info.bPair && !bEstablished && pairSession == session)
    {
        pairSession.clear();
        pairTarget.clear();
        qInfo() << "Sync: pairing with" << info.address << "failed:" << status << reason;
        emit pairFinished(false, PairFailureText(status, reason));
        emit pairingChanged();
    }

    if (!info.manualAddress.isEmpty())
    {
        for (ManualTarget& target : manualTargets)
        {
            if (target.address != info.manualAddress)
            {
                continue;
            }
            if (target.dialing == session)
            {
                target.dialing.clear();
            }
            if (!bEstablished)
            {
                target.error = ConnectFailureText(status, reason);
                if (!session->PeerId().isEmpty())
                {
                    target.peerId = session->PeerId();
                    target.peerName = session->PeerName();
                }
                target.backoffMs = IsRejection(reason) ? RejectedBackoffMs : NextBackoff(target.backoffMs);
                target.nextDialMs = now + target.backoffMs;
            }
            else
            {
                target.nextDialMs = std::max(target.nextDialMs, now + Jitter(1000, 4000));
            }
        }
        emit manualAddressesChanged();
    }

    if (!peerId.isEmpty())
    {
        const auto linkIt = links.find(peerId);
        if (linkIt != links.end())
        {
            PeerLink& link = *linkIt;
            const bool bWasDialing = link.dialing == session;
            if (bWasDialing)
            {
                link.dialing.clear();
            }
            if (bWasDialing && !bEstablished)
            {
                if (IsRejection(reason))
                {
                    link.bRejected = true;
                    link.round.clear();
                    link.candidate = 0;
                    link.backoffMs = RejectedBackoffMs;
                    link.nextDialMs = now + RejectedBackoffMs;
                }
                else if (link.candidate < link.round.size())
                {
                    link.nextDialMs = now + CandidateGapMs;
                }
                else
                {
                    link.round.clear();
                    link.candidate = 0;
                    link.backoffMs = NextBackoff(link.backoffMs);
                    link.nextDialMs = now + link.backoffMs;
                }
            }
            if (bEstablished && link.active == session)
            {
                link.active.clear();
                link.round.clear();
                link.candidate = 0;
                if (reason == QLatin1String("shutdown"))
                {
                    link.backoffMs = MinBackoffMs;
                    link.nextDialMs = now + MinBackoffMs;
                }
                else
                {
                    link.backoffMs = 0;
                    link.nextDialMs = std::max(link.nextDialMs, now + Jitter(1000, 4000));
                }
            }
        }
        if (bEstablished && reason == QLatin1String("unpaired"))
        {
            Unpair(peerId);
        }
    }
    emit devicesChanged();
}

void SyncSubsystem::OnFrame(SyncSession* session, const proto::Frame& frame)
{
    if (!frame.hasEndpoints() || !engine)
    {
        return;
    }
    const auto infoIt = sessions.constFind(session);
    if (infoIt == sessions.cend())
    {
        return;
    }
    const QString peerId = session->PeerId();
    const auto linkIt = links.constFind(peerId);
    if (linkIt == links.cend() || linkIt->active != session)
    {
        return;
    }

    QStringList addresses;
    const auto add = [&addresses](const QString& address)
    {
        if (!address.isEmpty() && !addresses.contains(address) && addresses.size() < MaxPeerAddresses)
        {
            addresses.append(address);
        }
    };
    if (session->IsClient())
    {
        add(infoIt->address);
    }
    for (const QString& announced : frame.endpoints().addresses())
    {
        if (const auto parsed = ParseAddress(announced, 0))
        {
            add(FormatAddress(parsed->host, parsed->port));
        }
    }
    if (!session->IsClient() && session->PeerListenPort() != 0 && !session->PeerHost().isEmpty())
    {
        add(FormatAddress(session->PeerHost(), session->PeerListenPort()));
    }
    engine->SetPeerAddresses(peerId, addresses);
}

void SyncSubsystem::SendEndpoints(SyncSession* session)
{
    if (!network || !session || !session->IsEstablished())
    {
        return;
    }
    proto::Endpoints endpoints;
    endpoints.setAddresses(network->Endpoints());
    proto::Frame frame;
    frame.setEndpoints(endpoints);
    session->SendFrame(frame);
}

void SyncSubsystem::OnNetworkChanged()
{
    if (network)
    {
        const QStringList endpoints = network->Endpoints();
        if (endpoints != sentEndpoints)
        {
            sentEndpoints = endpoints;
            for (auto it = links.cbegin(); it != links.cend(); ++it)
            {
                if (it->active && it->active->IsEstablished())
                {
                    SendEndpoints(it->active);
                }
            }
            ResetDialBackoff();
        }
    }
    emit networkChanged();
}

void SyncSubsystem::ResetDialBackoff()
{
    const qint64 now = NowMs();
    for (auto it = links.begin(); it != links.end(); ++it)
    {
        if (it->active || it->bRejected)
        {
            continue;
        }
        it->round.clear();
        it->candidate = 0;
        it->backoffMs = 0;
        it->nextDialMs = now + Jitter(500, 2500);
    }
    for (ManualTarget& target : manualTargets)
    {
        if (target.dialing)
        {
            continue;
        }
        target.backoffMs = 0;
        target.nextDialMs = now + Jitter(500, 2500);
    }
}

void SyncSubsystem::DialTick()
{
    if (!engine)
    {
        return;
    }
    const qint64 now = NowMs();
    const QStringList peerIds = engine->GetState().peers.keys();
    for (const QString& peerId : peerIds)
    {
        if (IsBusyWith(peerId) || now < links[peerId].nextDialMs)
        {
            continue;
        }
        DialPeer(peerId);
    }
    for (qsizetype i = 0; i < manualTargets.size(); ++i)
    {
        ManualTarget& target = manualTargets[i];
        if (target.dialing || now < target.nextDialMs)
        {
            continue;
        }
        if (!target.peerId.isEmpty() && engine->GetState().peers.contains(target.peerId) && IsBusyWith(target.peerId))
        {
            continue;
        }
        DialManual(target);
    }
}

void SyncSubsystem::OnBeacon(const QString& peerId)
{
    if (!engine || !engine->GetState().peers.contains(peerId) || IsBusyWith(peerId))
    {
        return;
    }
    PeerLink& link = links[peerId];
    const qint64 now = NowMs();
    if (link.bRejected || now - link.beaconDialMs < BeaconDialGapMs)
    {
        return;
    }
    link.beaconDialMs = now;
    link.round.clear();
    link.candidate = 0;
    DialPeer(peerId);
}

void SyncSubsystem::DialPeer(const QString& peerId)
{
    PeerLink& link = links[peerId];
    const qint64 now = NowMs();
    if (link.candidate >= link.round.size())
    {
        link.round = CandidatesFor(peerId);
        link.candidate = 0;
        if (link.round.isEmpty())
        {
            link.nextDialMs = now + MinBackoffMs;
            return;
        }
    }
    while (link.candidate < link.round.size())
    {
        const auto parsed = ParseAddress(link.round.at(link.candidate++), 0);
        if (!parsed)
        {
            continue;
        }
        SessionInfo info;
        info.address = FormatAddress(parsed->host, parsed->port);
        info.peerId = peerId;
        SyncSession* session = Dial(parsed->host, parsed->port, false, {}, peerId);
        link.dialing = session;
        Watch(session, info);
        emit devicesChanged();
        return;
    }
    link.round.clear();
    link.candidate = 0;
    link.backoffMs = NextBackoff(link.backoffMs);
    link.nextDialMs = now + link.backoffMs;
}

QStringList SyncSubsystem::CandidatesFor(const QString& peerId) const
{
    QStringList candidates;
    const auto add = [&candidates](const QString& address)
    {
        if (!address.isEmpty() && !candidates.contains(address))
        {
            candidates.append(address);
        }
    };
    if (network)
    {
        const auto& discovered = network->GetDiscovered();
        const auto it = discovered.constFind(peerId);
        if (it != discovered.cend() && it->port != 0 && !it->host.isEmpty())
        {
            add(FormatAddress(it->host, it->port));
        }
    }
    if (engine)
    {
        const QStringList addresses = engine->GetState().peers.value(peerId).addresses;
        for (const QString& address : addresses)
        {
            add(address);
        }
    }
    return candidates;
}

void SyncSubsystem::DialManual(ManualTarget& target)
{
    SessionInfo info;
    info.address = target.address;
    info.manualAddress = target.address;
    SyncSession* session = Dial(target.host, target.port, false, {}, {});
    target.dialing = session;
    Watch(session, info);
    emit manualAddressesChanged();
}

bool SyncSubsystem::IsBusyWith(const QString& peerId) const
{
    const auto link = links.constFind(peerId);
    if (link != links.cend())
    {
        if ((link->active && !link->active->IsClosed()) || (link->dialing && !link->dialing->IsClosed()))
        {
            return true;
        }
    }
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        const SyncSession* session = it.key();
        if (!session->IsClosed() && (session->PeerId() == peerId || it->peerId == peerId))
        {
            return true;
        }
    }
    return false;
}

void SyncSubsystem::RebuildManualTargets()
{
    const QStringList stored =
        QTomlUtils::FindPropertyValue<QStringList>(AppConfigs::Settings, Keys::SyncManualAddresses).value_or(QStringList());
    QList<ManualTarget> rebuilt;
    for (const QString& entry : stored)
    {
        const auto parsed = ParseAddress(entry, DefaultPort);
        if (!parsed)
        {
            continue;
        }
        const bool bDuplicate = std::any_of(rebuilt.cbegin(), rebuilt.cend(), [&parsed](const ManualTarget& target)
        {
            return SameAddress(HostPort{target.host, target.port}, *parsed);
        });
        if (bDuplicate)
        {
            continue;
        }
        const QString address = FormatAddress(parsed->host, parsed->port);
        ManualTarget target;
        for (const ManualTarget& old : std::as_const(manualTargets))
        {
            if (old.address == address)
            {
                target = old;
                break;
            }
        }
        target.address = address;
        target.host = parsed->host;
        target.port = parsed->port;
        rebuilt.append(target);
        if (rebuilt.size() >= MaxManualAddresses)
        {
            break;
        }
    }
    for (const ManualTarget& old : std::as_const(manualTargets))
    {
        const bool bKept = std::any_of(rebuilt.cbegin(), rebuilt.cend(), [&old](const ManualTarget& target)
        {
            return target.address == old.address;
        });
        if (!bKept && old.dialing && !old.dialing->IsEstablished() && !old.dialing->IsClosed())
        {
            old.dialing->Close(QStringLiteral("cancelled"));
        }
    }
    manualTargets = rebuilt;
    emit manualAddressesChanged();
}

void SyncSubsystem::SyncLinksWithPeers()
{
    if (!engine)
    {
        return;
    }
    const auto& peers = engine->GetState().peers;
    for (auto it = links.begin(); it != links.end();)
    {
        if (peers.contains(it.key()))
        {
            ++it;
            continue;
        }
        const QPointer<SyncSession> dialing = it->dialing;
        const QPointer<SyncSession> active = it->active;
        it = links.erase(it);
        if (dialing && !dialing->IsClosed())
        {
            dialing->Close(QStringLiteral("cancelled"));
        }
        if (active && !active->IsClosed())
        {
            active->Close(QStringLiteral("unpaired"));
        }
    }
}

void SyncSubsystem::Unpair(const QString& peerId)
{
    if (!engine || !engine->GetState().peers.contains(peerId))
    {
        return;
    }
    const auto linkIt = links.find(peerId);
    if (linkIt != links.end())
    {
        const QPointer<SyncSession> active = linkIt->active;
        const QPointer<SyncSession> dialing = linkIt->dialing;
        links.erase(linkIt);
        if (active && !active->IsClosed())
        {
            active->Close(QStringLiteral("unpaired"));
        }
        if (dialing && !dialing->IsClosed())
        {
            dialing->Close(QStringLiteral("cancelled"));
        }
    }
    QList<QPointer<SyncSession>> others;
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        if (!it.key()->IsClosed() && (it.key()->PeerId() == peerId || it->peerId == peerId))
        {
            others.append(it.key());
        }
    }
    for (const QPointer<SyncSession>& session : std::as_const(others))
    {
        if (session && !session->IsClosed())
        {
            session->Close(session->IsEstablished() ? QStringLiteral("unpaired") : QStringLiteral("cancelled"));
        }
    }
    engine->RemovePeer(peerId);
    for (ManualTarget& target : manualTargets)
    {
        if (target.peerId == peerId)
        {
            target.peerId.clear();
            target.peerName.clear();
        }
    }
    qInfo() << "Sync: unpaired" << peerId;
    emit devicesChanged();
    emit manualAddressesChanged();
}

QList<SyncSubsystem::DeviceInfo> SyncSubsystem::GetDevices() const
{
    QList<DeviceInfo> devices;
    if (!engine)
    {
        return devices;
    }
    static const QHash<QString, SyncNetwork::Discovered> NoDevices;
    const QHash<QString, SyncNetwork::Discovered>& discovered = network ? network->GetDiscovered() : NoDevices;
    const auto& peers = engine->GetState().peers;
    const qint64 now = NowMs();
    const auto byName = [](const DeviceInfo& a, const DeviceInfo& b)
    {
        const int order = a.name.compare(b.name, Qt::CaseInsensitive);
        return order != 0 ? order < 0 : a.id < b.id;
    };

    for (const Sync::Peer& peer : peers)
    {
        DeviceInfo info;
        info.id = peer.id;
        info.name = peer.name.isEmpty() ? peer.id.left(8) : peer.name;
        info.platform = peer.platform;
        info.bPaired = true;
        const PeerLink link = links.value(peer.id);
        info.bConnected = engine->IsConnected(peer.id);
        info.bConnecting = !info.bConnected && IsBusyWith(peer.id);
        info.bRejected = link.bRejected;
        const auto nearby = discovered.constFind(peer.id);
        info.bNearby = nearby != discovered.cend();
        info.bPairing = info.bNearby && nearby->bPairing;
        SyncSession* active = link.active.data();
        if (info.bConnected && active)
        {
            info.address = active->IsClient() ? sessions.value(active).address : active->PeerHost();
            info.lastSeenMs = now;
        }
        else
        {
            info.lastSeenMs = info.bConnected ? now : peer.lastSeenMs;
            if (info.bNearby)
            {
                info.address = nearby->host;
            }
            else if (!peer.addresses.isEmpty())
            {
                info.address = peer.addresses.first();
            }
        }
        devices.append(info);
    }
    std::sort(devices.begin(), devices.end(), byName);

    QList<DeviceInfo> unpaired;
    for (const SyncNetwork::Discovered& device : discovered)
    {
        if (device.deviceId == deviceId || peers.contains(device.deviceId))
        {
            continue;
        }
        DeviceInfo info;
        info.id = device.deviceId;
        info.name = device.name.isEmpty() ? device.deviceId.left(8) : device.name;
        info.platform = device.platform;
        info.bNearby = true;
        info.bPairing = device.bPairing;
        info.address = device.port != 0 ? FormatAddress(device.host, device.port) : device.host;
        unpaired.append(info);
    }
    std::sort(unpaired.begin(), unpaired.end(), byName);
    devices.append(unpaired);
    return devices;
}

QString SyncSubsystem::OpenPairing()
{
    if (!engine)
    {
        return QStringLiteral("Sync is off");
    }
    if (listenPort == 0)
    {
        return QStringLiteral("The sync server isn't running: %1").arg(listenError);
    }
    badCodes = 0;
    codeRegenerations = 0;
    pairingCode = NewPairingCode();
    pairingExpiresMs = NowMs() + PairingLifetimeMs;
    pairingTimer->start(static_cast<int>(PairingLifetimeMs));
    network->SetBeacon(DeviceName(), true);
    qInfo() << "Sync: pairing opened";
    emit pairingChanged();
    return {};
}

void SyncSubsystem::ClosePairing()
{
    pairingTimer->stop();
    const bool bWasOpen = !pairingCode.isEmpty();
    pairingCode.clear();
    pairingExpiresMs = 0;
    badCodes = 0;
    codeRegenerations = 0;
    if (network)
    {
        network->SetBeacon(DeviceName(), false);
    }
    if (bWasOpen)
    {
        emit pairingChanged();
    }
}

QString SyncSubsystem::PairWith(const QString& target, const QString& code)
{
    if (!engine)
    {
        return QStringLiteral("Sync is off");
    }
    QString digits = code;
    digits.remove(u' ');
    digits.remove(u'-');
    const bool bDigits = std::all_of(digits.cbegin(), digits.cend(), [](QChar c)
    {
        return c >= u'0' && c <= u'9';
    });
    if (digits.size() != 6 || !bDigits)
    {
        return QStringLiteral("The code has 6 digits");
    }
    if (pairSession)
    {
        return QStringLiteral("Pairing is already in progress");
    }

    const QString trimmed = target.trimmed();
    QString host;
    quint16 port = 0;
    QString expected;
    if (Sync::IsDeviceId(trimmed))
    {
        if (trimmed == deviceId)
        {
            return QStringLiteral("That's this device");
        }
        const auto& discovered = network->GetDiscovered();
        const auto it = discovered.constFind(trimmed);
        if (it == discovered.cend() || it->port == 0 || it->host.isEmpty())
        {
            return QStringLiteral("The device is no longer nearby");
        }
        host = it->host;
        port = it->port;
        expected = trimmed;
    }
    else
    {
        const auto parsed = ParseAddress(trimmed, DefaultPort);
        if (!parsed)
        {
            return QStringLiteral("Not a valid address");
        }
        host = parsed->host;
        port = parsed->port;
    }

    SessionInfo info;
    info.address = FormatAddress(host, port);
    info.bPair = true;
    SyncSession* session = Dial(host, port, true, digits, expected);
    Watch(session, info);
    pairSession = session;
    pairTarget = trimmed;
    qInfo() << "Sync: pairing with" << info.address;
    emit pairingChanged();
    return {};
}

void SyncSubsystem::CancelPairWith()
{
    const QPointer<SyncSession> session = pairSession;
    pairSession.clear();
    pairTarget.clear();
    if (session && !session->IsEstablished() && !session->IsClosed())
    {
        session->Close(QStringLiteral("cancelled"));
    }
    emit pairingChanged();
}

QString SyncSubsystem::NewPairingCode() const
{
    return QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(1000000), 6, 10, QLatin1Char('0'));
}

void SyncSubsystem::OnBadCode()
{
    if (pairingCode.isEmpty() || ++badCodes < MaxBadCodes)
    {
        return;
    }
    badCodes = 0;
    if (++codeRegenerations >= MaxCodeRegenerations)
    {
        qWarning() << "Sync: too many wrong pairing codes, pairing closed";
        ClosePairing();
        return;
    }
    pairingCode = NewPairingCode();
    pairingExpiresMs = NowMs() + PairingLifetimeMs;
    pairingTimer->start(static_cast<int>(PairingLifetimeMs));
    qWarning() << "Sync: too many wrong pairing codes, the code was replaced";
    emit pairingChanged();
}

QList<SyncSubsystem::AddressInfo> SyncSubsystem::GetManualAddresses() const
{
    QList<AddressInfo> addresses;
    for (const ManualTarget& target : manualTargets)
    {
        AddressInfo info;
        info.address = target.address;
        if (engine && !target.peerId.isEmpty())
        {
            const auto& peers = engine->GetState().peers;
            const auto peer = peers.constFind(target.peerId);
            if (peer != peers.cend() && !peer->name.isEmpty())
            {
                info.deviceName = peer->name;
            }
        }
        if (info.deviceName.isEmpty())
        {
            info.deviceName = target.peerName;
        }
        info.bConnecting = target.dialing && !target.dialing->IsEstablished() && !target.dialing->IsClosed();
        info.bConnected = engine && !target.peerId.isEmpty() && engine->IsConnected(target.peerId);
        if (!info.bConnected && !info.bConnecting)
        {
            info.error = target.error;
        }
        addresses.append(info);
    }
    return addresses;
}

QString SyncSubsystem::AddManualAddress(const QString& address)
{
    const auto parsed = ParseAddress(address, DefaultPort);
    if (!parsed)
    {
        return QStringLiteral("Enter an address like 203.0.113.5:47777, a host name or [IPv6]:port");
    }
    if (listenPort != 0)
    {
        const QHostAddress ip(parsed->host);
        bool bOwn = parsed->port == listenPort && (ip.isLoopback() || parsed->host == QLatin1String("localhost"));
        if (!bOwn && network)
        {
            QStringList own = network->LocalEndpoints();
            own.append(network->GetPublicEndpoint());
            bOwn = std::any_of(own.cbegin(), own.cend(), [&parsed](const QString& endpoint)
            {
                const auto local = ParseAddress(endpoint, 0);
                return local && SameAddress(*local, *parsed);
            });
        }
        if (bOwn)
        {
            return QStringLiteral("That's this device's own address");
        }
    }

    QStringList stored =
        QTomlUtils::FindPropertyValue<QStringList>(AppConfigs::Settings, Keys::SyncManualAddresses).value_or(QStringList());
    for (const QString& entry : std::as_const(stored))
    {
        const auto existing = ParseAddress(entry, DefaultPort);
        if (existing && SameAddress(*existing, *parsed))
        {
            return QStringLiteral("This address is already in the list");
        }
    }
    if (stored.size() >= MaxManualAddresses)
    {
        return QStringLiteral("The list is full: remove an address first");
    }
    stored.append(FormatAddress(parsed->host, parsed->port));
    QTomlUtils::SetPropertyValue(AppConfigs::Settings, Keys::SyncManualAddresses, stored);
    RebuildManualTargets();
    return {};
}

void SyncSubsystem::RemoveManualAddress(const QString& address)
{
    QStringList stored =
        QTomlUtils::FindPropertyValue<QStringList>(AppConfigs::Settings, Keys::SyncManualAddresses).value_or(QStringList());
    const auto parsed = ParseAddress(address, DefaultPort);
    const qsizetype before = stored.size();
    stored.removeIf([&](const QString& entry)
    {
        if (entry == address)
        {
            return true;
        }
        const auto existing = ParseAddress(entry, DefaultPort);
        return parsed && existing && SameAddress(*existing, *parsed);
    });
    if (stored.size() == before)
    {
        return;
    }
    QTomlUtils::SetPropertyValue(AppConfigs::Settings, Keys::SyncManualAddresses, stored);
    RebuildManualTargets();
}

SyncSubsystem::Reachability SyncSubsystem::GetReachability() const
{
    if (!network || listenPort == 0)
    {
        return Reachability::Unknown;
    }
    switch (network->GetReachability())
    {
    case SyncNetwork::Reachability::Public:
        return Reachability::Public;
    case SyncNetwork::Reachability::Mapped:
        return Reachability::Mapped;
    case SyncNetwork::Reachability::Private:
        return Reachability::Private;
    case SyncNetwork::Reachability::Unknown:
        break;
    }
    return Reachability::Unknown;
}

SyncSubsystem::UpnpState SyncSubsystem::GetUpnpState() const
{
    if (!network || listenPort == 0)
    {
        return UpnpState::Off;
    }
    switch (network->GetUpnpState())
    {
    case SyncNetwork::UpnpState::Searching:
        return UpnpState::Searching;
    case SyncNetwork::UpnpState::Mapped:
        return UpnpState::Mapped;
    case SyncNetwork::UpnpState::Failed:
        return UpnpState::Failed;
    case SyncNetwork::UpnpState::Off:
        break;
    }
    return UpnpState::Off;
}

QString SyncSubsystem::PublicEndpoint() const
{
    return network ? network->GetPublicEndpoint() : QString();
}

QString SyncSubsystem::DetectedAddress() const
{
    return network ? network->GetDetectedAddress() : QString();
}

QStringList SyncSubsystem::LocalEndpoints() const
{
    return network ? network->LocalEndpoints() : QStringList();
}

bool SyncSubsystem::IsHearingBeacons() const
{
    return !network || network->IsHearingBeacons();
}

QList<SyncSubsystem::ConflictInfo> SyncSubsystem::GetConflicts() const
{
    QList<ConflictInfo> conflicts;
    if (!engine)
    {
        return conflicts;
    }
    for (const Sync::Conflict& conflict : engine->GetConflicts())
    {
        conflicts.append(Describe(conflict, engine->GetState()));
    }
    return conflicts;
}

void SyncSubsystem::Resolve(const QStringList& ids, Choice choice)
{
    if (!engine || ids.isEmpty())
    {
        return;
    }
    QStringList applicable = ids;
    if (choice == Choice::Both)
    {
        const QSet<QString> wanted(ids.cbegin(), ids.cend());
        applicable.clear();
        for (const Sync::Conflict& conflict : engine->GetConflicts())
        {
            if (wanted.contains(conflict.id) && !Describe(conflict, engine->GetState()).bothLabel.isEmpty())
            {
                applicable.append(conflict.id);
            }
        }
    }
    if (applicable.isEmpty())
    {
        return;
    }
    const Sync::Choice mapped = choice == Choice::Mine     ? Sync::Choice::Mine
                                : choice == Choice::Theirs ? Sync::Choice::Theirs
                                                           : Sync::Choice::Both;
    engine->Resolve(applicable, mapped);
}

QList<SyncSubsystem::TransferInfo> SyncSubsystem::GetTransfers() const
{
    QList<TransferInfo> transfers;
    if (!engine)
    {
        return transfers;
    }
    for (const SyncEngine::Transfer& transfer : engine->GetTransfers())
    {
        TransferInfo info;
        info.title = transfer.title;
        info.peerName = transfer.peerName;
        info.size = transfer.size;
        info.done = transfer.done;
        transfers.append(info);
    }
    return transfers;
}

SyncSubsystem::Status SyncSubsystem::GetStatus() const
{
    Status status;
    if (!engine)
    {
        return status;
    }
    status.bRunning = true;
    const auto& peers = engine->GetState().peers;
    for (auto it = peers.cbegin(); it != peers.cend(); ++it)
    {
        if (engine->IsConnected(it.key()))
        {
            ++status.connected;
        }
    }
    const SyncEngine::Progress progress = engine->GetProgress();
    status.hashing = progress.hashing;
    status.missingFiles = progress.missingFiles;
    status.missingBytes = progress.missingBytes;
    status.transfers = progress.transfers;
    status.transferBytes = progress.transferBytes;
    status.transferDone = progress.transferDone;
    status.deferred = progress.deferred;
    status.blocked = progress.blocked;
    status.conflicts = static_cast<int>(engine->GetConflicts().size());
    return status;
}

void SyncSubsystem::Reindex()
{
    if (engine)
    {
        engine->Reindex();
    }
}

bool SyncSubsystem::IsFileSyncAvailable() const
{
    return engine && engine->IsFileSyncAvailable();
}

QString SyncSubsystem::PlacementRoot() const
{
    return engine ? engine->GetPlacementRoot() : QString();
}
