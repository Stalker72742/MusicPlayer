//
// Created by Stalker7274 on 19.11.2026.
//

#include "SyncEngine.h"

#include "SyncSession.h"
#include "sync.qpb.h"

#include "../Library/FileScanner.h"
#include "Library/LibrarySubsystem.h"
#include "Player/PlayerSubsystem.h"
#include "Playlists/PlaylistSubsystem.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QRandomGenerator>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace proto = soundlink::sync::v1;

namespace
{
    constexpr int SaveDelayMs = 2000;
    constexpr int PushDelayMs = 300;
    constexpr int ProgressDelayMs = 250;
    constexpr int LibraryDelayMs = 1000;
    constexpr int PlaylistDelayMs = 500;
    constexpr int HousekeepingIntervalMs = 5000;
    constexpr qsizetype MaxBatchRecords = 256;
    constexpr qsizetype MaxBatchBytes = 512 * 1024;
    constexpr qint64 PushBackpressureBytes = 12 * 1024 * 1024;
    constexpr int BackpressureRetryMs = 20;
    constexpr qint64 MaxClockSkewMs = 24LL * 60 * 60 * 1000;
    constexpr qint64 SameTrackDurationMs = 2000;
    constexpr qsizetype MaxPeerAddresses = 16;
    constexpr qsizetype MaxPeerAddressLength = 64;

    qint64 NowMs()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }

    void Kick(QTimer& timer)
    {
        if (!timer.isActive())
        {
            timer.start();
        }
    }

    QString LockPathFor(const QString& statePath)
    {
        return statePath + QStringLiteral(".running");
    }

    QString RandomDeviceId()
    {
        quint32 words[4];
        QRandomGenerator::system()->fillRange(words);
        return Sync::ToHex(QByteArray(reinterpret_cast<const char*>(words), sizeof(words)));
    }

    quint64 NewEpoch()
    {
        return (QRandomGenerator::system()->generate64() & 0x7FFFFFFFFFFFFFFFULL) | 1;
    }

    QString FileNameOf(const QString& path)
    {
        return path.mid(path.lastIndexOf(u'/') + 1);
    }

    qsizetype TextSize(const QString& text)
    {
        return text.size() * 3 + 2;
    }

    qsizetype InfoSize(const Sync::TrackInfo& info)
    {
        return TextSize(info.title) + TextSize(info.artist) + TextSize(info.album) + 12;
    }

    qsizetype MapSize(const QMap<QString, QString>& map)
    {
        qsizetype size = 0;
        for (auto it = map.cbegin(); it != map.cend(); ++it)
        {
            size += TextSize(it.key()) + TextSize(it.value()) + 4;
        }
        return size;
    }

    qsizetype EstimateSize(const Sync::Record& record)
    {
        qsizetype size = 96 + record.uid.size() + record.vv.size() * 48;
        switch (record.kind)
        {
        case Sync::Kind::File:
            size += 64 + TextSize(record.file.relPath) + record.file.history.size() * 34 + InfoSize(record.file.info) +
                    record.file.onlineUid.size();
            break;
        case Sync::Kind::Online:
            size += TextSize(record.online.pageUrl) + TextSize(record.online.thumbnailUrl) +
                    TextSize(record.online.channelUrl) + InfoSize(record.online.info);
            break;
        case Sync::Kind::Meta:
            for (const QString& label : record.meta.labels)
            {
                size += TextSize(label);
            }
            size += MapSize(record.meta.tagEdits);
            break;
        case Sync::Kind::Playlist:
            size += TextSize(record.playlist.name) + TextSize(record.playlist.smart) + MapSize(record.playlist.extras);
            for (const QString& uid : record.playlist.tracks)
            {
                size += uid.size() + 2;
            }
            break;
        }
        return size;
    }

    bool SamePlaylist(const Sync::PlaylistBody& a, const Sync::PlaylistBody& b)
    {
        return a.name == b.name && a.bSmart == b.bSmart && a.tracks == b.tracks && a.smart == b.smart &&
               a.extras == b.extras;
    }

    QString WithSuffix(const QString& relPath, const QString& suffix)
    {
        const qsizetype slash = relPath.lastIndexOf(u'/');
        const qsizetype dot = relPath.lastIndexOf(u'.');
        if (dot > slash + 1)
        {
            return relPath.left(dot) + suffix + relPath.mid(dot);
        }
        return relPath + suffix;
    }
}

SyncEngine::SyncEngine(QObject* parent)
    : QObject(parent)
{
    pool.setMaxThreadCount(3);
    for (QTimer* timer : {&saveTimer, &pushTimer, &libraryTimer, &playlistTimer, &pumpTimer, &progressTimer})
    {
        timer->setSingleShot(true);
    }
    saveTimer.setInterval(SaveDelayMs);
    pushTimer.setInterval(PushDelayMs);
    progressTimer.setInterval(ProgressDelayMs);
    libraryTimer.setInterval(LibraryDelayMs);
    playlistTimer.setInterval(PlaylistDelayMs);
    pumpTimer.setInterval(0);
    housekeepingTimer.setInterval(HousekeepingIntervalMs);

    connect(&saveTimer, &QTimer::timeout, this, &SyncEngine::Save);
    connect(&pushTimer, &QTimer::timeout, this, [this]
    {
        const QStringList peers = links.keys();
        for (const QString& peerId : peers)
        {
            StartCycle(peerId, false);
        }
    });
    connect(&progressTimer, &QTimer::timeout, this, &SyncEngine::progressChanged);
    connect(&libraryTimer, &QTimer::timeout, this, &SyncEngine::RunLibraryPass);
    connect(&playlistTimer, &QTimer::timeout, this, &SyncEngine::RunPlaylistPass);
    connect(&pumpTimer, &QTimer::timeout, this, &SyncEngine::PumpDownloads);
    connect(&housekeepingTimer, &QTimer::timeout, this, &SyncEngine::Housekeeping);
}

SyncEngine::~SyncEngine()
{
    CancelBackgroundWork();
}

void SyncEngine::Load(const QString& path)
{
    statePath = path;
    const bool bUnclean = QFile::exists(LockPathFor(path));
    bool bLoaded = false;
    if (QFile::exists(path))
    {
        bLoaded = Sync::LoadState(path, state);
        if (!bLoaded)
        {
            const QString corrupted = path + QStringLiteral(".corrupted");
            QFile::remove(corrupted);
            QFile::rename(path, corrupted);
            qWarning() << "Sync: unreadable state moved to" << corrupted;
        }
    }
    if (!bLoaded)
    {
        state = Sync::State();
        state.deviceId = RandomDeviceId();
        state.epoch = NewEpoch();
    }
    else if (bUnclean)
    {
        state.epoch = NewEpoch();
        qInfo() << "Sync: the last run did not stop cleanly, starting a new database epoch";
    }
    RebuildIndexes();
    Save();
}

void SyncEngine::Save()
{
    saveTimer.stop();
    if (statePath.isEmpty())
    {
        return;
    }
    if (!Sync::SaveState(statePath, state))
    {
        qWarning() << "Sync: cannot write" << statePath;
    }
}

void SyncEngine::Start()
{
    if (bStarted)
    {
        return;
    }
    bStarted = true;

    LibrarySubsystem& library = LibrarySubsystem::Get();
    connect(&library, &LibrarySubsystem::tracksChanged, this, [this]
    {
        Kick(libraryTimer);
    });
    connect(&library, &LibrarySubsystem::foldersChanged, this, [this]
    {
        UpdatePlacementRoot(LibraryRoots());
        Kick(libraryTimer);
    });
    connect(&library, &LibrarySubsystem::scanStateChanged, this, [this]
    {
        if (!LibrarySubsystem::Get().IsScanning())
        {
            bLibraryReady = true;
            Kick(libraryTimer);
        }
    });
    connect(&PlaylistSubsystem::Get(), &PlaylistSubsystem::playlistsChanged, this, [this]
    {
        Kick(playlistTimer);
    });
    connect(&PlayerSubsystem::Get(), &PlayerSubsystem::currentTrackChanged, this, &SyncEngine::RetryDeferred);

    UpdatePlacementRoot(LibraryRoots());
    bLibraryReady = !library.IsScanning();
    if (!statePath.isEmpty())
    {
        QFile lock(LockPathFor(statePath));
        if (lock.open(QIODevice::WriteOnly))
        {
            lock.close();
        }
    }
    housekeepingTimer.start();
    libraryTimer.start();
    playlistTimer.start();
}

void SyncEngine::Stop()
{
    CancelBackgroundWork();
    const QStringList keys = downloads.keys();
    for (const QString& key : keys)
    {
        AbortDownload(key, true);
    }
    const QStringList peers = links.keys();
    for (const QString& peerId : peers)
    {
        DetachSession(peerId, links.value(peerId).session);
    }
    served.clear();
    candidates.clear();
    activeHashes = 0;
    if (bStarted)
    {
        disconnect(&LibrarySubsystem::Get(), nullptr, this, nullptr);
        disconnect(&PlaylistSubsystem::Get(), nullptr, this, nullptr);
        disconnect(&PlayerSubsystem::Get(), nullptr, this, nullptr);
    }
    for (QTimer* timer : {&saveTimer, &pushTimer, &libraryTimer, &playlistTimer, &pumpTimer, &progressTimer,
                          &housekeepingTimer})
    {
        timer->stop();
    }
    bStarted = false;
    Save();
    if (!statePath.isEmpty())
    {
        QFile::remove(LockPathFor(statePath));
    }
}

void SyncEngine::CancelBackgroundWork()
{
    for (const Candidate& candidate : std::as_const(candidates))
    {
        if (candidate.cancel)
        {
            candidate.cancel->store(true);
        }
    }
    for (const std::shared_ptr<Download>& download : std::as_const(downloads))
    {
        if (download->cancel)
        {
            download->cancel->store(true);
        }
    }
    pool.clear();
    pool.waitForDone();
}

proto::Welcome SyncEngine::MakeWelcome(const QString& peerId, const QString& deviceName) const
{
    proto::Welcome welcome;
    welcome.setDeviceName(deviceName);
    welcome.setEpoch(state.epoch);
    const auto it = state.peers.constFind(peerId);
    if (it != state.peers.cend())
    {
        welcome.setYourEpoch(it->peerEpoch);
        welcome.setYourSeq(it->appliedSeq);
    }
    return welcome;
}

void SyncEngine::AttachSession(SyncSession* session)
{
    if (!session || !session->IsEstablished())
    {
        return;
    }
    const QString peerId = session->PeerId();
    if (!state.peers.contains(peerId))
    {
        return;
    }
    if (links.contains(peerId))
    {
        DetachSession(peerId, links.value(peerId).session);
    }

    const proto::Welcome& welcome = session->PeerWelcome();
    Sync::Peer& peer = state.peers[peerId];
    Link link;
    link.session = session;
    link.peerId = peerId;
    link.peerEpoch = welcome.epoch();
    if (peer.peerEpoch != welcome.epoch())
    {
        link.bIgnoreEcho = peer.peerEpoch != 0;
        peer.peerEpoch = welcome.epoch();
        peer.appliedSeq = 0;
    }
    if (welcome.yourEpoch() == state.epoch)
    {
        if (welcome.yourSeq() <= state.seq)
        {
            link.sentSeq = welcome.yourSeq();
        }
        else
        {
            state.seq = welcome.yourSeq();
        }
    }
    const QString name = !welcome.deviceName().isEmpty() ? welcome.deviceName() : session->PeerName();
    if (!name.isEmpty())
    {
        peer.name = name.left(128);
    }
    peer.platform = session->PeerPlatform();
    peer.lastSeenMs = NowMs();
    links.insert(peerId, link);

    connect(session, &SyncSession::frameReceived, this, [this, peerId, session](const proto::Frame& frame)
    {
        const auto it = links.constFind(peerId);
        if (it != links.cend() && it->session == session)
        {
            OnFrame(peerId, frame);
        }
    });
    connect(session, &SyncSession::closed, this, [this, peerId, session]
    {
        DetachSession(peerId, session);
    });

    StartCycle(peerId, true);
    ScheduleSave();
    SchedulePump();
    emit peersChanged();
}

bool SyncEngine::IsConnected(const QString& peerId) const
{
    const auto it = links.constFind(peerId);
    return it != links.cend() && it->session && it->session->IsEstablished();
}

void SyncEngine::DetachSession(const QString& peerId, SyncSession* session)
{
    const auto it = links.find(peerId);
    if (it == links.end() || it->session != session)
    {
        return;
    }
    if (session)
    {
        disconnect(session, nullptr, this, nullptr);
    }
    QStringList keys;
    for (auto download = downloads.cbegin(); download != downloads.cend(); ++download)
    {
        if ((*download)->peerId == peerId)
        {
            keys.append(download.key());
        }
    }
    links.erase(it);
    for (const QString& key : std::as_const(keys))
    {
        AbortDownload(key, true);
    }
    const auto peer = state.peers.find(peerId);
    if (peer != state.peers.end())
    {
        peer->lastSeenMs = NowMs();
    }
    ScheduleSave();
    SchedulePump();
    ScheduleProgress();
    emit peersChanged();
}

void SyncEngine::UpsertPeer(const Sync::Peer& peer)
{
    if (!Sync::IsDeviceId(peer.id) || peer.id == state.deviceId)
    {
        return;
    }
    Sync::Peer& stored = state.peers[peer.id];
    stored.id = peer.id;
    if (!peer.name.isEmpty())
    {
        stored.name = peer.name.left(128);
    }
    if (peer.platform != 0)
    {
        stored.platform = peer.platform;
    }
    if (!peer.psk.isEmpty())
    {
        stored.psk = peer.psk;
    }
    stored.lastSeenMs = std::max(stored.lastSeenMs, peer.lastSeenMs);
    Save();
    if (!peer.addresses.isEmpty())
    {
        SetPeerAddresses(peer.id, peer.addresses);
    }
    emit peersChanged();
}

void SyncEngine::RemovePeer(const QString& peerId)
{
    if (links.contains(peerId))
    {
        DetachSession(peerId, links.value(peerId).session);
    }
    state.peers.remove(peerId);
    for (Sync::Entry& entry : state.entries)
    {
        entry.haveBy.remove(peerId);
    }
    Save();
    SchedulePump();
    ScheduleProgress();
    emit peersChanged();
}

void SyncEngine::SetPeerAddresses(const QString& peerId, const QStringList& addresses)
{
    const auto it = state.peers.find(peerId);
    if (it == state.peers.end())
    {
        return;
    }
    QStringList clean;
    for (const QString& address : addresses)
    {
        const QString trimmed = address.trimmed();
        if (!trimmed.isEmpty() && trimmed.size() <= MaxPeerAddressLength && !clean.contains(trimmed))
        {
            clean.append(trimmed);
            if (clean.size() >= MaxPeerAddresses)
            {
                break;
            }
        }
    }
    if (clean == it->addresses)
    {
        return;
    }
    it->addresses = clean;
    ScheduleSave();
    emit peersChanged();
}

void SyncEngine::StartCycle(const QString& peerId, bool bForced)
{
    const auto it = links.find(peerId);
    if (it == links.end())
    {
        return;
    }
    Link& link = *it;
    if (link.bCycleRunning)
    {
        link.bCycleAgain = true;
        return;
    }
    if (!bForced && link.sentSeq >= state.seq)
    {
        return;
    }
    link.cycleSeq = state.seq;
    QList<std::pair<quint64, QString>> pending;
    for (auto entry = state.entries.cbegin(); entry != state.entries.cend(); ++entry)
    {
        if (entry->seq > link.sentSeq && entry->seq <= link.cycleSeq)
        {
            pending.append({entry->seq, entry.key()});
        }
    }
    std::sort(pending.begin(), pending.end());
    link.cycleKeys.clear();
    link.cycleKeys.reserve(pending.size());
    for (const auto& item : std::as_const(pending))
    {
        link.cycleKeys.append(item.second);
    }
    link.cycleIndex = 0;
    link.bCycleRunning = true;
    link.bCycleAgain = false;
    ContinueCycle(peerId);
}

void SyncEngine::ContinueCycle(const QString& peerId)
{
    const auto it = links.find(peerId);
    if (it == links.end())
    {
        return;
    }
    Link& link = *it;
    link.bCycleWaiting = false;
    SyncSession* session = link.session;
    if (!link.bCycleRunning || !session || !session->IsEstablished())
    {
        return;
    }
    while (link.cycleIndex < link.cycleKeys.size())
    {
        if (session->PendingBytes() > PushBackpressureBytes)
        {
            link.bCycleWaiting = true;
            QTimer::singleShot(BackpressureRetryMs, this, [this, peerId]
            {
                ContinueCycle(peerId);
            });
            return;
        }
        QList<proto::SyncRecord> records;
        qsizetype bytes = 0;
        while (link.cycleIndex < link.cycleKeys.size() && records.size() < MaxBatchRecords && bytes < MaxBatchBytes)
        {
            const QString key = link.cycleKeys.at(link.cycleIndex++);
            const auto entry = state.entries.constFind(key);
            if (entry == state.entries.cend() || entry->seq <= link.sentSeq)
            {
                continue;
            }
            if (entry->seq > link.cycleSeq)
            {
                link.bCycleAgain = true;
                continue;
            }
            if (!link.bIgnoreEcho && entry->echoPeer == peerId)
            {
                continue;
            }
            Sync::Record record = entry->record;
            record.bHave = record.kind == Sync::Kind::File && !record.bDeleted && entry->haveBy.contains(state.deviceId);
            bytes += EstimateSize(record);
            records.append(Sync::ToProto(record));
        }
        if (!records.isEmpty())
        {
            proto::IndexBatch batch;
            batch.setRecords(std::move(records));
            proto::Frame frame;
            frame.setIndex(std::move(batch));
            session->SendFrame(frame);
        }
    }

    proto::IndexDone done;
    done.setSeq(link.cycleSeq);
    proto::Frame frame;
    frame.setIndexDone(std::move(done));
    session->SendFrame(frame);

    link.sentSeq = link.cycleSeq;
    link.bCycleRunning = false;
    link.bIgnoreEcho = false;
    link.cycleKeys.clear();
    link.cycleIndex = 0;
    if (link.bCycleAgain || link.sentSeq < state.seq)
    {
        link.bCycleAgain = false;
        SchedulePush();
    }
}

void SyncEngine::OnFrame(const QString& peerId, const proto::Frame& frame)
{
    if (frame.hasIndex())
    {
        for (const proto::SyncRecord& message : frame.index().records())
        {
            std::optional<Sync::Record> record = Sync::FromProto(message);
            if (record)
            {
                MergeRecord(peerId, std::move(*record));
            }
        }
        AfterBatch();
    }
    else if (frame.hasIndexDone())
    {
        const auto peer = state.peers.find(peerId);
        if (peer != state.peers.end())
        {
            peer->appliedSeq = frame.indexDone().seq();
            peer->lastSeenMs = NowMs();
        }
        const auto link = links.find(peerId);
        if (link != links.end())
        {
            link->bInitialReceived = true;
        }
        ScheduleSave();
        AfterBatch();
        emit peersChanged();
    }
    else if (frame.hasBlockRequest())
    {
        ServeBlock(peerId, frame.blockRequest());
    }
    else if (frame.hasBlockData())
    {
        OnBlockData(peerId, frame.blockData());
    }
}

bool SyncEngine::AllLinksCaughtUp() const
{
    for (const Link& link : links)
    {
        if (!link.bInitialReceived)
        {
            return false;
        }
    }
    return true;
}

void SyncEngine::ScheduleSave()
{
    Kick(saveTimer);
}

void SyncEngine::SchedulePush()
{
    if (!links.isEmpty())
    {
        Kick(pushTimer);
    }
}

void SyncEngine::ScheduleProgress()
{
    Kick(progressTimer);
}

quint64 SyncEngine::NextHlc()
{
    const quint64 wall = quint64(std::max<qint64>(0, NowMs())) << 16;
    state.hlc = std::max(state.hlc + 1, wall);
    return state.hlc;
}

void SyncEngine::ObserveHlc(quint64 hlc)
{
    const quint64 limit = quint64(std::max<qint64>(0, NowMs() + MaxClockSkewMs)) << 16;
    if (hlc > state.hlc && hlc <= limit)
    {
        state.hlc = hlc;
    }
}

void SyncEngine::Touch(const QString& key)
{
    const auto it = state.entries.find(key);
    if (it == state.entries.end())
    {
        return;
    }
    it->seq = ++state.seq;
    it->echoPeer.clear();
    SchedulePush();
    ScheduleSave();
}

void SyncEngine::Commit(const QString& key, Sync::Record record)
{
    record.bHave = false;
    if (Sync::Conflict* conflict = FindConflict(key))
    {
        const Sync::Record stored = state.entries.value(key).record;
        switch (conflict->type)
        {
        case Sync::ConflictType::RemoteChange:
        {
            const Sync::Record theirs = conflict->theirs;
            const bool bSame = record.bDeleted == theirs.bDeleted &&
                               (record.bDeleted || record.file.sha256 == theirs.file.sha256);
            if (bSame)
            {
                const QString peerId = conflict->peerId;
                RemoveConflict(conflict->id);
                Adopt(key, theirs, peerId);
                return;
            }
            conflict->type = record.bDeleted != theirs.bDeleted ? Sync::ConflictType::DeleteVsEdit
                                                                : Sync::ConflictType::Content;
            [[fallthrough]];
        }
        case Sync::ConflictType::Content:
        case Sync::ConflictType::DeleteVsEdit:
        case Sync::ConflictType::Playlist:
            record.vv = stored.vv;
            record.hlc = NextHlc();
            record.changedBy = state.deviceId;
            conflict->mine = record;
            ConflictsModified();
            ScheduleSave();
            return;
        case Sync::ConflictType::LocalDelete:
            RemoveConflict(conflict->id);
            break;
        case Sync::ConflictType::Name:
            break;
        }
    }

    const auto existing = state.entries.constFind(key);
    if (existing != state.entries.cend())
    {
        record.vv = Sync::Merged(record.vv, existing->record.vv);
    }
    const quint64 hlc = NextHlc();
    const quint64 own = record.vv.value(state.deviceId, 0);
    record.vv.insert(state.deviceId, std::max(own + 1, hlc));
    record.hlc = hlc;
    record.changedBy = state.deviceId;

    Sync::Entry& entry = PutRecord(key, record);
    if (record.kind == Sync::Kind::File)
    {
        if (!record.bDeleted && HoldsCurrent(key))
        {
            entry.haveBy.insert(state.deviceId);
        }
        else
        {
            entry.haveBy.remove(state.deviceId);
        }
    }
    entry.seq = ++state.seq;
    entry.echoPeer.clear();
    if (record.kind == Sync::Kind::File)
    {
        RefreshNeed(key);
    }
    SchedulePush();
    ScheduleSave();
    ScheduleProgress();
}

Sync::Entry& SyncEngine::PutRecord(const QString& key, const Sync::Record& record)
{
    const Sync::Record next = record;
    Sync::Entry& entry = state.entries[key];
    const Sync::Record& old = entry.record;
    const bool bExisted = !old.uid.isEmpty();
    if (bExisted && old.kind == Sync::Kind::File)
    {
        if (old.bDeleted)
        {
            keysByOldSha.remove(old.file.sha256, key);
        }
        else
        {
            keysBySha.remove(old.file.sha256, key);
        }
        for (const QByteArray& sha : old.file.history)
        {
            keysByOldSha.remove(sha, key);
        }
    }
    const bool bContentChanged = !bExisted || old.bDeleted != next.bDeleted || old.file.sha256 != next.file.sha256;
    if (next.kind == Sync::Kind::File && bExisted && old.file.onlineUid != next.file.onlineUid &&
        filesByUid.contains(next.uid))
    {
        bUidIndexDirty = true;
    }
    entry.record = next;
    entry.record.bHave = false;
    if (next.kind == Sync::Kind::File)
    {
        if (bContentChanged || next.bDeleted)
        {
            entry.haveBy.clear();
        }
        if (next.bDeleted)
        {
            keysByOldSha.insert(next.file.sha256, key);
        }
        else
        {
            keysBySha.insert(next.file.sha256, key);
        }
        for (const QByteArray& sha : next.file.history)
        {
            keysByOldSha.insert(sha, key);
        }
    }
    return entry;
}

void SyncEngine::SetSelfHave(const QString& key, bool bHave)
{
    const auto it = state.entries.find(key);
    if (it == state.entries.end() || it->record.kind != Sync::Kind::File)
    {
        return;
    }
    if (it->record.bDeleted)
    {
        bHave = false;
    }
    if (it->haveBy.contains(state.deviceId) == bHave)
    {
        return;
    }
    if (bHave)
    {
        it->haveBy.insert(state.deviceId);
    }
    else
    {
        it->haveBy.remove(state.deviceId);
    }
    Touch(key);
    RefreshNeed(key);
    ScheduleProgress();
}

void SyncEngine::RecomputeSelfHave(const QString& uid)
{
    if (uid.isEmpty())
    {
        return;
    }
    const QString key = Sync::MakeKey(Sync::Kind::File, uid);
    if (state.entries.contains(key))
    {
        SetSelfHave(key, HoldsCurrent(key));
    }
}

bool SyncEngine::HoldsCurrent(const QString& key) const
{
    const auto it = state.entries.constFind(key);
    if (it == state.entries.cend() || it->record.kind != Sync::Kind::File || it->record.bDeleted)
    {
        return false;
    }
    const QByteArray& sha = it->record.file.sha256;
    const auto range = filesByUid.equal_range(it->record.uid);
    for (auto copy = range.first; copy != range.second; ++copy)
    {
        const auto local = state.files.constFind(copy.value());
        if (local != state.files.cend() && local->sha == sha)
        {
            return true;
        }
    }
    return false;
}

void SyncEngine::RefreshNeed(const QString& key)
{
    const auto it = state.entries.constFind(key);
    bool bNeed = false;
    if (it != state.entries.cend() && it->record.kind == Sync::Kind::File && !it->record.bDeleted)
    {
        bNeed = !it->haveBy.contains(state.deviceId) && !state.parked.contains(key) &&
                !state.pendingNameChecks.contains(key) && !conflictedKeys.contains(key) && IsFileSyncAvailable() &&
                FileScanner::IsAudioFile(it->record.file.relPath);
    }
    if (bNeed)
    {
        if (!needContent.contains(key))
        {
            needContent.insert(key);
            SchedulePump();
            ScheduleProgress();
        }
    }
    else if (needContent.remove(key))
    {
        if (downloads.contains(key))
        {
            AbortDownload(key, true);
        }
        ScheduleProgress();
    }
}

void SyncEngine::RebuildIndexes()
{
    keysBySha.clear();
    keysByOldSha.clear();
    localBySha.clear();
    filesByUid.clear();
    needContent.clear();
    for (auto it = state.entries.cbegin(); it != state.entries.cend(); ++it)
    {
        const Sync::Record& record = it->record;
        if (record.kind != Sync::Kind::File)
        {
            continue;
        }
        if (record.bDeleted)
        {
            keysByOldSha.insert(record.file.sha256, it.key());
        }
        else
        {
            keysBySha.insert(record.file.sha256, it.key());
        }
        for (const QByteArray& sha : record.file.history)
        {
            keysByOldSha.insert(sha, it.key());
        }
    }
    for (auto it = state.files.cbegin(); it != state.files.cend(); ++it)
    {
        if (!it->sha.isEmpty())
        {
            localBySha.insert(it->sha, it.key());
        }
        if (!it->uid.isEmpty())
        {
            filesByUid.insert(it->uid, it.key());
        }
    }
    conflictedKeys.clear();
    for (const Sync::Conflict& conflict : std::as_const(state.conflicts))
    {
        conflictedKeys.insert(conflict.key);
        if (conflict.type == Sync::ConflictType::Name)
        {
            conflictedKeys.insert(conflict.otherKey);
        }
    }
    const QStringList keys = state.entries.keys();
    for (const QString& key : keys)
    {
        RefreshNeed(key);
    }
}

void SyncEngine::PutLocal(const QString& pathKey, const Sync::LocalFile& local)
{
    const Sync::LocalFile next = local;
    QString oldUid;
    const auto old = state.files.constFind(pathKey);
    if (old == state.files.cend() || old->uid != next.uid)
    {
        bUidIndexDirty = true;
        if (bStarted)
        {
            Kick(libraryTimer);
        }
    }
    if (old != state.files.cend())
    {
        oldUid = old->uid;
        if (!old->sha.isEmpty())
        {
            localBySha.remove(old->sha, pathKey);
        }
        if (!old->uid.isEmpty())
        {
            filesByUid.remove(old->uid, pathKey);
        }
    }
    state.files.insert(pathKey, next);
    if (!next.sha.isEmpty())
    {
        localBySha.insert(next.sha, pathKey);
    }
    if (!next.uid.isEmpty())
    {
        filesByUid.insert(next.uid, pathKey);
    }
    if (oldUid != next.uid)
    {
        RecomputeSelfHave(oldUid);
    }
    RecomputeSelfHave(next.uid);
    ScheduleSave();
}

void SyncEngine::RemoveLocal(const QString& pathKey)
{
    const auto it = state.files.find(pathKey);
    if (it == state.files.end())
    {
        return;
    }
    const QString uid = it->uid;
    if (!it->sha.isEmpty())
    {
        localBySha.remove(it->sha, pathKey);
    }
    if (!uid.isEmpty())
    {
        filesByUid.remove(uid, pathKey);
    }
    state.files.erase(it);
    bUidIndexDirty = true;
    if (bStarted)
    {
        Kick(libraryTimer);
    }
    RecomputeSelfHave(uid);
    ScheduleSave();
}

QString SyncEngine::PrimaryCopy(const QString& uid) const
{
    const auto entry = state.entries.constFind(Sync::MakeKey(Sync::Kind::File, uid));
    const QByteArray sha = entry != state.entries.cend() ? entry->record.file.sha256 : QByteArray();
    QString current;
    QString any;
    const auto range = filesByUid.equal_range(uid);
    for (auto copy = range.first; copy != range.second; ++copy)
    {
        const QString& pathKey = copy.value();
        if (any.isEmpty() || pathKey < any)
        {
            any = pathKey;
        }
        const auto local = state.files.constFind(pathKey);
        if (!sha.isEmpty() && local != state.files.cend() && local->sha == sha && (current.isEmpty() || pathKey < current))
        {
            current = pathKey;
        }
    }
    return current.isEmpty() ? any : current;
}

QString SyncEngine::PeerName(const QString& peerId) const
{
    const auto it = state.peers.constFind(peerId);
    if (it != state.peers.cend() && !it->name.isEmpty())
    {
        return it->name;
    }
    return peerId.left(8);
}

bool SyncEngine::Wins(const Sync::Record& a, const Sync::Record& b)
{
    if (a.hlc != b.hlc)
    {
        return a.hlc > b.hlc;
    }
    return a.changedBy > b.changedBy;
}

void SyncEngine::MergeRecord(const QString& peerId, Sync::Record record)
{
    ObserveHlc(record.hlc);
    const QString key = record.Key();
    if (record.kind != Sync::Kind::File)
    {
        FlushLocal(key);
    }
    auto it = state.entries.find(key);
    if (it == state.entries.end())
    {
        Adopt(key, record, peerId);
        return;
    }
    if (it->record.kind != record.kind)
    {
        return;
    }
    const Sync::Order order = Sync::Compare(record.vv, it->record.vv);
    if (order == Sync::Order::Older)
    {
        Touch(key);
        return;
    }
    if (order == Sync::Order::Equal)
    {
        if (record.kind == Sync::Kind::File && !it->record.bDeleted && it->haveBy.contains(peerId) != record.bHave)
        {
            if (record.bHave)
            {
                it->haveBy.insert(peerId);
            }
            else
            {
                it->haveBy.remove(peerId);
            }
            ScheduleSave();
            SchedulePump();
        }
        return;
    }
    if (Sync::Conflict* conflict = FindConflict(key))
    {
        if (!MergeIntoConflict(*conflict, record, peerId))
        {
            return;
        }
        it = state.entries.find(key);
        if (it == state.entries.end())
        {
            Adopt(key, record, peerId);
            return;
        }
    }
    if (order == Sync::Order::Newer)
    {
        if (NeedsConfirmation(it->record, record))
        {
            const Sync::Record current = it->record;
            AddConflict(Sync::ConflictType::RemoteChange, key, {}, peerId, current, record);
        }
        else
        {
            Adopt(key, record, peerId);
        }
        return;
    }
    MergeConcurrent(key, record, peerId);
}

bool SyncEngine::MergeIntoConflict(Sync::Conflict& conflict, const Sync::Record& record, const QString& peerId)
{
    const QString id = conflict.id;
    const QString key = conflict.key;
    const auto updateTheirs = [this, &conflict, &record, &peerId]
    {
        conflict.theirs = record;
        conflict.peerId = peerId;
        conflict.peerName = PeerName(peerId);
        ConflictsModified();
        ScheduleSave();
    };

    switch (conflict.type)
    {
    case Sync::ConflictType::LocalDelete:
    {
        const Sync::Order order = Sync::Compare(record.vv, state.entries.value(key).record.vv);
        if (order == Sync::Order::Newer && !record.bDeleted)
        {
            updateTheirs();
            Adopt(key, record, peerId);
            return false;
        }
        RemoveConflict(id);
        return true;
    }
    case Sync::ConflictType::RemoteChange:
    {
        const Sync::Order order = Sync::Compare(record.vv, conflict.theirs.vv);
        if (order == Sync::Order::Older || order == Sync::Order::Equal)
        {
            return false;
        }
        if (!record.bDeleted && !conflict.mine.bDeleted && record.file.sha256 == conflict.mine.file.sha256)
        {
            RemoveConflict(id);
            return true;
        }
        updateTheirs();
        return false;
    }
    case Sync::ConflictType::Content:
    case Sync::ConflictType::DeleteVsEdit:
    case Sync::ConflictType::Playlist:
    {
        const Sync::Order againstTheirs = Sync::Compare(record.vv, conflict.theirs.vv);
        const auto stored = state.entries.constFind(key);
        const bool bLocalEdit = stored != state.entries.cend() && conflict.mine.hlc != stored->record.hlc;
        if (!bLocalEdit && againstTheirs == Sync::Order::Newer &&
            Sync::Compare(record.vv, conflict.mine.vv) == Sync::Order::Newer)
        {
            RemoveConflict(id);
            return true;
        }
        if (againstTheirs == Sync::Order::Newer)
        {
            updateTheirs();
        }
        return false;
    }
    case Sync::ConflictType::Name:
        return true;
    }
    return false;
}

bool SyncEngine::NeedsConfirmation(const Sync::Record& current, const Sync::Record& incoming) const
{
    if (!bConfirmRemote || current.kind != Sync::Kind::File || current.bDeleted || !filesByUid.contains(current.uid))
    {
        return false;
    }
    return incoming.bDeleted || incoming.file.sha256 != current.file.sha256;
}

void SyncEngine::Adopt(const QString& key, const Sync::Record& record, const QString& peerId)
{
    const auto existing = state.entries.constFind(key);
    const Sync::Record before = existing != state.entries.cend() ? existing->record : Sync::Record();
    Sync::Entry& entry = PutRecord(key, record);
    const bool bAliveFile = record.kind == Sync::Kind::File && !record.bDeleted;
    if (bAliveFile && !peerId.isEmpty())
    {
        if (record.bHave)
        {
            entry.haveBy.insert(peerId);
        }
        else
        {
            entry.haveBy.remove(peerId);
        }
    }
    entry.seq = ++state.seq;
    entry.echoPeer = bAliveFile && entry.haveBy.contains(state.deviceId) ? QString() : peerId;
    if (record.kind == Sync::Kind::File)
    {
        RecomputeSelfHave(record.uid);
        RefreshNeed(key);
    }
    AfterChange(key, before, peerId);
    SchedulePush();
    ScheduleSave();
    ScheduleProgress();
}

void SyncEngine::MergeConcurrent(const QString& key, const Sync::Record& record, const QString& peerId)
{
    const Sync::Record mine = state.entries.value(key).record;
    const bool bTheirsWin = Wins(record, mine);
    const Sync::Record& winner = bTheirsWin ? record : mine;
    const Sync::Record& loser = bTheirsWin ? mine : record;
    Sync::Record merged = winner;
    merged.vv = Sync::Merged(mine.vv, record.vv);
    merged.hlc = std::max(mine.hlc, record.hlc);
    merged.bHave = false;

    switch (record.kind)
    {
    case Sync::Kind::File:
        if (mine.bDeleted != record.bDeleted)
        {
            AddConflict(Sync::ConflictType::DeleteVsEdit, key, {}, peerId, mine, record);
            Touch(key);
            return;
        }
        if (!mine.bDeleted && mine.file.sha256 != record.file.sha256)
        {
            const bool bTheirsDescend = record.file.history.contains(mine.file.sha256);
            const bool bMineDescend = mine.file.history.contains(record.file.sha256);
            if (bTheirsDescend == bMineDescend)
            {
                AddConflict(Sync::ConflictType::Content, key, {}, peerId, mine, record);
                Touch(key);
                return;
            }
            if (bTheirsDescend)
            {
                Sync::Record forward = record;
                forward.vv = merged.vv;
                if (NeedsConfirmation(mine, forward))
                {
                    AddConflict(Sync::ConflictType::RemoteChange, key, {}, peerId, mine, forward);
                }
                else
                {
                    Adopt(key, forward, peerId);
                    Touch(key);
                }
                return;
            }
            const Sync::VersionVector vv = merged.vv;
            merged = mine;
            merged.vv = vv;
            merged.hlc = std::max(mine.hlc, record.hlc);
            merged.bHave = false;
        }
        break;
    case Sync::Kind::Online:
        if (mine.bDeleted != record.bDeleted)
        {
            const Sync::Record& alive = mine.bDeleted ? record : mine;
            merged.online = alive.online;
            merged.bDeleted = false;
            merged.changedBy = alive.changedBy;
            break;
        }
        if (merged.online.channelUrl.isEmpty())
        {
            merged.online.channelUrl = loser.online.channelUrl;
        }
        if (merged.online.thumbnailUrl.isEmpty())
        {
            merged.online.thumbnailUrl = loser.online.thumbnailUrl;
        }
        break;
    case Sync::Kind::Meta:
        for (const QString& label : loser.meta.labels)
        {
            if (!merged.meta.labels.contains(label))
            {
                merged.meta.labels.append(label);
            }
        }
        merged.meta.tagEdits = loser.meta.tagEdits;
        merged.meta.tagEdits.insert(winner.meta.tagEdits);
        break;
    case Sync::Kind::Playlist:
        if (mine.bDeleted != record.bDeleted || (!mine.bDeleted && !SamePlaylist(mine.playlist, record.playlist)))
        {
            AddConflict(Sync::ConflictType::Playlist, key, {}, peerId, mine, record);
            Touch(key);
            return;
        }
        break;
    }

    Sync::Entry& entry = PutRecord(key, merged);
    if (record.kind == Sync::Kind::File && !merged.bDeleted && !peerId.isEmpty())
    {
        if (record.bHave && merged.file.sha256 == record.file.sha256)
        {
            entry.haveBy.insert(peerId);
        }
        else
        {
            entry.haveBy.remove(peerId);
        }
    }
    Touch(key);
    if (record.kind == Sync::Kind::File)
    {
        RecomputeSelfHave(merged.uid);
        RefreshNeed(key);
    }
    AfterChange(key, mine, peerId);
    ScheduleProgress();
}

void SyncEngine::AfterChange(const QString& key, const Sync::Record& before, const QString& peerId)
{
    const auto it = state.entries.constFind(key);
    if (it == state.entries.cend())
    {
        return;
    }
    const Sync::Record record = it->record;
    const bool bExisted = !before.uid.isEmpty();
    switch (record.kind)
    {
    case Sync::Kind::File:
        if (record.bDeleted)
        {
            state.pendingNameChecks.remove(key);
            state.placements.remove(key);
            state.parked.remove(key);
            if (bExisted && !before.bDeleted)
            {
                ApplyFileTombstone(key, peerId);
            }
        }
        else if ((!bExisted || before.bDeleted) && !filesByUid.contains(record.uid))
        {
            state.pendingNameChecks.insert(key);
        }
        RefreshNeed(key);
        break;
    case Sync::Kind::Online:
        ApplyOnline(key);
        break;
    case Sync::Kind::Meta:
        ApplyMeta(key);
        break;
    case Sync::Kind::Playlist:
        ApplyPlaylist(key, false);
        break;
    }
}

void SyncEngine::AfterBatch()
{
    RecheckNameConflicts();
    RunNameChecks();
    FlushNotifications();
    SchedulePump();
    SchedulePush();
    ScheduleSave();
    ScheduleProgress();
}

Sync::Conflict* SyncEngine::FindConflict(const QString& key)
{
    if (!conflictedKeys.contains(key))
    {
        return nullptr;
    }
    for (Sync::Conflict& conflict : state.conflicts)
    {
        if (conflict.type != Sync::ConflictType::Name && conflict.key == key)
        {
            return &conflict;
        }
    }
    return nullptr;
}

bool SyncEngine::HasConflict(const QString& key) const
{
    return conflictedKeys.contains(key);
}

void SyncEngine::AddConflict(Sync::ConflictType type, const QString& key, const QString& otherKey,
                             const QString& peerId, const Sync::Record& mine, const Sync::Record& theirs,
                             const QString& localPath)
{
    Sync::Conflict conflict;
    conflict.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conflict.type = type;
    conflict.key = key;
    conflict.otherKey = otherKey;
    conflict.peerId = peerId;
    conflict.peerName = peerId.isEmpty() ? QString() : PeerName(peerId);
    conflict.mine = mine;
    conflict.theirs = theirs;
    conflict.createdMs = NowMs();
    conflict.localPath = localPath;
    state.conflicts.append(conflict);
    ++newConflicts;
    ConflictsModified();
    RefreshNeed(key);
    if (!otherKey.isEmpty())
    {
        RefreshNeed(otherKey);
    }
    ScheduleSave();
}

void SyncEngine::RemoveConflict(const QString& id)
{
    for (qsizetype i = 0; i < state.conflicts.size(); ++i)
    {
        if (state.conflicts.at(i).id != id)
        {
            continue;
        }
        const Sync::Conflict conflict = state.conflicts.takeAt(i);
        ConflictsModified();
        RefreshNeed(conflict.key);
        if (!conflict.otherKey.isEmpty())
        {
            RefreshNeed(conflict.otherKey);
        }
        ScheduleSave();
        return;
    }
}

void SyncEngine::ConflictsModified()
{
    conflictedKeys.clear();
    for (const Sync::Conflict& conflict : std::as_const(state.conflicts))
    {
        conflictedKeys.insert(conflict.key);
        if (conflict.type == Sync::ConflictType::Name)
        {
            conflictedKeys.insert(conflict.otherKey);
        }
    }
    if (!bConflictsDirty)
    {
        bConflictsDirty = true;
        QTimer::singleShot(0, this, [this]
        {
            bConflictsDirty = false;
            emit conflictsChanged();
        });
    }
}

void SyncEngine::Resolve(const QStringList& ids, Sync::Choice choice)
{
    for (const QString& id : ids)
    {
        const auto it = std::find_if(state.conflicts.cbegin(), state.conflicts.cend(), [&id](const Sync::Conflict& c)
        {
            return c.id == id;
        });
        if (it == state.conflicts.cend())
        {
            continue;
        }
        if (it->type == Sync::ConflictType::Playlist)
        {
            const QString key = it->key;
            FlushLocal(key);
        }
        const auto current = std::find_if(state.conflicts.cbegin(), state.conflicts.cend(), [&id](const Sync::Conflict& c)
        {
            return c.id == id;
        });
        if (current != state.conflicts.cend())
        {
            const Sync::Conflict conflict = *current;
            ResolveOne(conflict, choice);
        }
    }
    AfterBatch();
}

void SyncEngine::TombstoneItem(const QString& key)
{
    const auto it = state.entries.constFind(key);
    if (it == state.entries.cend() || it->record.bDeleted)
    {
        return;
    }
    const Sync::Record before = it->record;
    Sync::Record record = before;
    record.bDeleted = true;
    Commit(key, record);
    AfterChange(key, before, {});
}

bool SyncEngine::ResolveOne(const Sync::Conflict& conflict, Sync::Choice choice)
{
    using Type = Sync::ConflictType;
    using Pick = Sync::Choice;
    const bool bBothAllowed = conflict.type == Type::Content || conflict.type == Type::Name || conflict.type == Type::Playlist;
    if (choice == Pick::Both && !bBothAllowed)
    {
        return false;
    }
    RemoveConflict(conflict.id);
    const Sync::VersionVector vv = Sync::Merged(conflict.mine.vv, conflict.theirs.vv);
    const auto markPeerHas = [this, &conflict](const QString& key)
    {
        const auto it = state.entries.find(key);
        if (it != state.entries.end() && conflict.theirs.bHave && !conflict.peerId.isEmpty() && !it->record.bDeleted &&
            it->record.file.sha256 == conflict.theirs.file.sha256)
        {
            it->haveBy.insert(conflict.peerId);
            SchedulePump();
        }
    };

    switch (conflict.type)
    {
    case Type::Content:
    case Type::DeleteVsEdit:
    {
        const Sync::Record before = state.entries.value(conflict.key).record;
        Sync::Record chosen = choice == Pick::Theirs ? conflict.theirs : conflict.mine;
        chosen.vv = vv;
        Commit(conflict.key, chosen);
        if (choice == Pick::Theirs)
        {
            markPeerHas(conflict.key);
        }
        AfterChange(conflict.key, before, {});
        if (choice == Pick::Both && !conflict.theirs.bDeleted)
        {
            Sync::Record copy = conflict.theirs;
            copy.uid = NewFileUid(conflict.theirs.file.sha256);
            copy.vv.clear();
            copy.bDeleted = false;
            copy.file.history.clear();
            const QString suffix = QStringLiteral(" (%1)").arg(conflict.peerName.isEmpty() ? PeerName(conflict.peerId) : conflict.peerName);
            copy.file.relPath = WithSuffix(conflict.theirs.file.relPath, suffix);
            const QString copyKey = copy.Key();
            Commit(copyKey, copy);
            markPeerHas(copyKey);
            AfterChange(copyKey, {}, {});
        }
        return true;
    }
    case Type::Name:
        if (choice == Pick::Both)
        {
            state.keptNamePairs.insert(conflict.key + u'|' + conflict.otherKey);
            state.pendingNameChecks.remove(conflict.otherKey);
            RefreshNeed(conflict.otherKey);
        }
        else if (choice == Pick::Mine)
        {
            TombstoneItem(conflict.otherKey);
        }
        else
        {
            if (!conflict.localPath.isEmpty())
            {
                state.placements.insert(conflict.otherKey, conflict.localPath);
            }
            TombstoneItem(conflict.key);
            state.pendingNameChecks.remove(conflict.otherKey);
            RefreshNeed(conflict.otherKey);
        }
        ScheduleSave();
        return true;
    case Type::Playlist:
    {
        Sync::Record chosen = choice == Pick::Theirs ? conflict.theirs : conflict.mine;
        if (choice == Pick::Both)
        {
            if (conflict.mine.bDeleted != conflict.theirs.bDeleted)
            {
                chosen = conflict.mine.bDeleted ? conflict.theirs : conflict.mine;
            }
            else if (!conflict.mine.bDeleted)
            {
                QSet<QString> present(chosen.playlist.tracks.cbegin(), chosen.playlist.tracks.cend());
                for (const QString& uid : conflict.theirs.playlist.tracks)
                {
                    if (!present.contains(uid))
                    {
                        present.insert(uid);
                        chosen.playlist.tracks.append(uid);
                    }
                }
            }
        }
        chosen.vv = vv;
        Commit(conflict.key, chosen);
        ApplyPlaylist(conflict.key, true);
        return true;
    }
    case Type::RemoteChange:
        if (choice == Pick::Theirs)
        {
            Adopt(conflict.key, conflict.theirs, conflict.peerId);
        }
        else
        {
            Sync::Record record = conflict.mine;
            record.vv = vv;
            Commit(conflict.key, record);
        }
        return true;
    case Type::LocalDelete:
        if (choice == Pick::Mine)
        {
            const auto it = state.entries.constFind(conflict.key);
            const Sync::Record before = it != state.entries.cend() ? it->record : conflict.mine;
            Sync::Record record = before;
            record.bDeleted = true;
            record.vv = Sync::Merged(vv, before.vv);
            Commit(conflict.key, record);
            AfterChange(conflict.key, before, {});
        }
        else
        {
            if (!conflict.localPath.isEmpty())
            {
                state.placements.insert(conflict.key, conflict.localPath);
            }
            RecomputeSelfHave(state.entries.value(conflict.key).record.uid);
            RefreshNeed(conflict.key);
            ScheduleSave();
        }
        return true;
    }
    return false;
}

void SyncEngine::RunNameChecks()
{
    if (state.pendingNameChecks.isEmpty() || !AllLinksCaughtUp() || !IsIndexIdle())
    {
        return;
    }
    QHash<QString, QStringList> byName;
    for (auto it = state.files.cbegin(); it != state.files.cend(); ++it)
    {
        byName[FileNameOf(it.key())].append(it.key());
    }

    const QStringList pending = state.pendingNameChecks.values();
    for (const QString& key : pending)
    {
        const auto entry = state.entries.constFind(key);
        if (entry == state.entries.cend() || entry->record.kind != Sync::Kind::File || entry->record.bDeleted)
        {
            state.pendingNameChecks.remove(key);
            RefreshNeed(key);
            continue;
        }
        const Sync::Record record = entry->record;
        if (filesByUid.contains(record.uid) || localBySha.contains(record.file.sha256))
        {
            state.pendingNameChecks.remove(key);
            RefreshNeed(key);
            continue;
        }
        if (placementRoot.isEmpty())
        {
            continue;
        }
        const QString target = state.placements.value(key, PlacementPathFor(record.file.relPath));
        if (target.isEmpty())
        {
            continue;
        }
        QString clashKey;
        QString clashPath;
        const auto held = state.files.constFind(Sync::PathKey(target));
        if (held != state.files.cend())
        {
            if (held->uid.isEmpty())
            {
                continue;
            }
            if (held->uid != record.uid)
            {
                clashKey = Sync::MakeKey(Sync::Kind::File, held->uid);
                clashPath = held->path;
            }
        }
        const Sync::TrackInfo& info = record.file.info;
        if (clashKey.isEmpty() && !info.title.isEmpty() && !info.artist.isEmpty())
        {
            const QStringList sameName = byName.value(FileNameOf(Sync::PathKey(target)));
            for (const QString& pathKey : sameName)
            {
                const Sync::LocalFile& local = state.files[pathKey];
                if (local.uid.isEmpty() || local.uid == record.uid)
                {
                    continue;
                }
                const auto other = state.entries.constFind(Sync::MakeKey(Sync::Kind::File, local.uid));
                if (other == state.entries.cend() || other->record.bDeleted)
                {
                    continue;
                }
                const Sync::TrackInfo& otherInfo = other->record.file.info;
                if (otherInfo.title.compare(info.title, Qt::CaseInsensitive) == 0 &&
                    otherInfo.artist.compare(info.artist, Qt::CaseInsensitive) == 0 &&
                    qAbs(otherInfo.durationMs - info.durationMs) <= SameTrackDurationMs)
                {
                    clashKey = other.key();
                    clashPath = local.path;
                    break;
                }
            }
        }
        state.pendingNameChecks.remove(key);
        if (!clashKey.isEmpty() && !state.keptNamePairs.contains(clashKey + u'|' + key) && state.entries.contains(clashKey))
        {
            AddConflict(Sync::ConflictType::Name, clashKey, key, record.changedBy, state.entries.value(clashKey).record,
                        record, clashPath);
        }
        RefreshNeed(key);
    }
    ScheduleSave();
}

void SyncEngine::RecheckNameConflicts()
{
    QList<Sync::Conflict> stale;
    for (const Sync::Conflict& conflict : std::as_const(state.conflicts))
    {
        if (conflict.type != Sync::ConflictType::Name)
        {
            continue;
        }
        const auto local = state.entries.constFind(conflict.key);
        const auto incoming = state.entries.constFind(conflict.otherKey);
        if (local == state.entries.cend() || incoming == state.entries.cend() || local->record.bDeleted ||
            incoming->record.bDeleted || !filesByUid.contains(local->record.uid))
        {
            stale.append(conflict);
        }
    }
    for (const Sync::Conflict& conflict : std::as_const(stale))
    {
        RemoveConflict(conflict.id);
        const auto incoming = state.entries.constFind(conflict.otherKey);
        if (incoming != state.entries.cend() && !incoming->record.bDeleted)
        {
            state.pendingNameChecks.insert(conflict.otherKey);
            RefreshNeed(conflict.otherKey);
        }
    }
}

void SyncEngine::FlushNotifications()
{
    if (newConflicts > 0)
    {
        const int count = std::exchange(newConflicts, 0);
        emit conflictsAdded(count);
    }
    if (!trashedBy.isEmpty())
    {
        const QHash<QString, int> trashed = std::exchange(trashedBy, {});
        for (auto it = trashed.cbegin(); it != trashed.cend(); ++it)
        {
            emit filesTrashed(it.value(), PeerName(it.key()));
        }
    }
}

QList<SyncEngine::Transfer> SyncEngine::GetTransfers() const
{
    QList<Transfer> result;
    result.reserve(downloads.size());
    for (const std::shared_ptr<Download>& download : downloads)
    {
        Transfer transfer;
        transfer.key = download->key;
        const auto entry = state.entries.constFind(download->key);
        if (entry != state.entries.cend())
        {
            const Sync::FileBody& file = entry->record.file;
            transfer.title = file.info.title.isEmpty() ? FileNameOf(file.relPath)
                                                       : (file.info.artist.isEmpty() ? file.info.title
                                                                                     : file.info.artist + QStringLiteral(" - ") + file.info.title);
        }
        transfer.peerName = download->peerId.isEmpty() ? QString() : PeerName(download->peerId);
        transfer.size = download->size;
        transfer.done = download->received;
        result.append(transfer);
    }
    std::sort(result.begin(), result.end(), [](const Transfer& a, const Transfer& b)
    {
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });
    return result;
}

SyncEngine::Progress SyncEngine::GetProgress() const
{
    Progress progress;
    progress.hashing = int(candidates.size());
    progress.missingFiles = int(needContent.size());
    for (const QString& key : needContent)
    {
        const auto entry = state.entries.constFind(key);
        if (entry != state.entries.cend())
        {
            progress.missingBytes += qint64(entry->record.file.size);
        }
    }
    progress.transfers = int(downloads.size());
    for (const std::shared_ptr<Download>& download : downloads)
    {
        progress.transferBytes += download->size;
        progress.transferDone += download->received;
    }
    progress.deferred = int(deferred.size());
    progress.blocked = blockedReason;
    return progress;
}
