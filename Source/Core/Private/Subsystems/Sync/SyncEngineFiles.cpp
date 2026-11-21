//
// Created by Stalker7274 on 20.11.2026.
//

#include "SyncEngine.h"

#include "AppConfigs.h"
#include "Library/LibrarySubsystem.h"
#include "Player/PlayerSubsystem.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>

#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace
{
    constexpr qint64 StabilityWindowMs = 15000;
    constexpr int MaxConcurrentHashes = 2;
    constexpr int MaxHashFailures = 3;
    constexpr qint64 HashRetryMs = 60000;
    constexpr qint64 ShortHashRetryMs = 2000;
    constexpr qint64 GoneDelayMs = 20000;
    constexpr qint64 MtimeToleranceMs = 2;
    constexpr qsizetype MassDeleteThreshold = 20;
    constexpr qsizetype MaxHistory = 16;
    constexpr qsizetype MaxPathSegments = 12;
    constexpr qsizetype MaxSegmentLength = 120;
    constexpr qsizetype MaxExtensionLength = 16;
    constexpr qsizetype ReadChunk = 1024 * 1024;

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

    QString FileNameOf(const QString& path)
    {
        return path.mid(path.lastIndexOf(u'/') + 1);
    }

    QString CleanPath(const QString& path)
    {
        if (path.trimmed().isEmpty())
        {
            return {};
        }
        return QDir::cleanPath(QFileInfo(QDir::fromNativeSeparators(path)).absoluteFilePath());
    }

    bool IsWithin(const QString& pathKey, const QString& rootKey)
    {
        if (rootKey.isEmpty())
        {
            return false;
        }
        if (pathKey == rootKey)
        {
            return true;
        }
        if (rootKey.endsWith(u'/'))
        {
            return pathKey.startsWith(rootKey);
        }
        return pathKey.size() > rootKey.size() && pathKey.startsWith(rootKey) && pathKey.at(rootKey.size()) == u'/';
    }

    QString Fingerprint(qint64 size, qint64 mtimeMs, const QString& name)
    {
        return QStringLiteral("%1|%2|%3").arg(size).arg(mtimeMs).arg(name);
    }

    bool IsStable(qint64 mtimeMs, qint64 observedMs, qint64 now)
    {
        return now - mtimeMs >= StabilityWindowMs || now - observedMs >= StabilityWindowMs;
    }

    bool SameTime(qint64 a, qint64 b)
    {
        return qAbs(a - b) <= MtimeToleranceMs;
    }

    qint64 ModifiedMs(const QFileInfo& info)
    {
        return info.lastModified().toMSecsSinceEpoch();
    }

    bool IsReservedName(const QString& name)
    {
        const QString stem = name.section(u'.', 0, 0).trimmed().toUpper();
        if (stem == QLatin1String("CON") || stem == QLatin1String("PRN") || stem == QLatin1String("AUX") ||
            stem == QLatin1String("NUL"))
        {
            return true;
        }
        if (stem.size() == 4 && (stem.startsWith(QLatin1String("COM")) || stem.startsWith(QLatin1String("LPT"))))
        {
            const QChar digit = stem.at(3);
            return digit >= u'1' && digit <= u'9';
        }
        return false;
    }

    QString CleanSegment(const QString& segment)
    {
        static const QString forbidden = QStringLiteral("<>:\"|?*\\/");
        QString result;
        result.reserve(segment.size());
        for (const QChar c : segment)
        {
            result.append(c.unicode() < 0x20 || forbidden.contains(c) ? QChar(u'_') : c);
        }
        if (result.size() > MaxSegmentLength)
        {
            const qsizetype dot = result.lastIndexOf(u'.');
            const QString extension = dot > 0 && result.size() - dot <= MaxExtensionLength ? result.mid(dot) : QString();
            qsizetype keep = MaxSegmentLength - extension.size();
            if (keep > 0 && result.at(keep - 1).isHighSurrogate())
            {
                --keep;
            }
            result = result.left(keep) + extension;
        }
        while (!result.isEmpty() && (result.endsWith(u'.') || result.endsWith(u' ')))
        {
            result.chop(1);
        }
        if (IsReservedName(result))
        {
            result.prepend(u'_');
        }
        return result;
    }
}

bool SyncEngine::OpenForReading(QFile& file, const QString& path)
{
#ifdef Q_OS_WIN
    QString native = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (native.startsWith(QLatin1String("\\\\")))
    {
        if (!native.startsWith(QLatin1String("\\\\?\\")))
        {
            native = QStringLiteral("\\\\?\\UNC\\") + native.mid(2);
        }
    }
    else if (native.size() >= 2 && native.at(1) == u':')
    {
        native.prepend(QStringLiteral("\\\\?\\"));
    }
    const HANDLE handle = CreateFileW(reinterpret_cast<const wchar_t*>(native.utf16()), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (fd == -1)
    {
        CloseHandle(handle);
        return false;
    }
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle))
    {
        _close(fd);
        return false;
    }
    return true;
#else
    file.setFileName(path);
    return file.open(QIODevice::ReadOnly);
#endif
}

QByteArray SyncEngine::HashFile(const QString& path, qint64 expectedSize, const std::atomic_bool& cancel)
{
    QFile file;
    if (!OpenForReading(file, path))
    {
        return {};
    }
    if (expectedSize >= 0 && file.size() != expectedSize)
    {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(ReadChunk, Qt::Uninitialized);
    qint64 total = 0;
    while (!cancel.load())
    {
        const qint64 read = file.read(buffer.data(), buffer.size());
        if (read < 0)
        {
            return {};
        }
        if (read == 0)
        {
            break;
        }
        hash.addData(QByteArrayView(buffer.constData(), read));
        total += read;
    }
    if (cancel.load() || (expectedSize >= 0 && total != expectedSize))
    {
        return {};
    }
    return hash.result();
}

void SyncEngine::Reindex()
{
    for (Candidate& candidate : candidates)
    {
        candidate.failures = 0;
        candidate.retryAtMs = 0;
    }
    failures.clear();
    retryAtMs.clear();
    LibrarySubsystem::Get().Rescan();
    if (bStarted)
    {
        Kick(libraryTimer);
        SchedulePump();
    }
}

QStringList SyncEngine::LibraryRoots() const
{
    QStringList result;
    QSet<QString> keys;
    const QStringList folders = LibrarySubsystem::Get().GetFolders();
    for (const QString& folder : folders)
    {
        const QString clean = CleanPath(folder);
        if (clean.isEmpty())
        {
            continue;
        }
        const QString key = Sync::PathKey(clean);
        if (!keys.contains(key))
        {
            keys.insert(key);
            result.append(clean);
        }
    }
    return result;
}

void SyncEngine::UpdatePlacementRoot(const QStringList& newRoots)
{
    roots = newRoots;
    QString next;
    if (!roots.isEmpty())
    {
        const QString folder = CleanPath(AppConfigs::GetString(AppConfigs::SettingsKeys::DownloadFolder));
        const QString folderKey = Sync::PathKey(folder);
        for (const QString& root : std::as_const(roots))
        {
            if (!folder.isEmpty() && IsWithin(folderKey, Sync::PathKey(root)))
            {
                next = folder;
                break;
            }
        }
        if (next.isEmpty())
        {
            next = QDir::cleanPath(roots.constFirst() + QStringLiteral("/SoundLink"));
        }
    }
    if (next == placementRoot)
    {
        return;
    }
    if (!next.isEmpty() && !placementRoot.isEmpty() && Sync::PathKey(next) == Sync::PathKey(placementRoot))
    {
        placementRoot = next;
        return;
    }

    const QString oldTemp = TempDir();
    placementRoot = next;
    const QStringList keys = downloads.keys();
    for (const QString& key : keys)
    {
        AbortDownload(key, false);
    }
    if (!oldTemp.isEmpty())
    {
        QDir(oldTemp).removeRecursively();
    }
    const QStringList entryKeys = state.entries.keys();
    for (const QString& key : entryKeys)
    {
        RefreshNeed(key);
    }
    RunNameChecks();
    SchedulePump();
    ScheduleProgress();
}

QString SyncEngine::RootOf(const QString& path) const
{
    const QString pathKey = Sync::PathKey(path);
    QString best;
    const auto consider = [&pathKey, &best](const QString& root)
    {
        const QString rootKey = Sync::PathKey(root);
        if (rootKey != pathKey && IsWithin(pathKey, rootKey) && root.size() > best.size())
        {
            best = root;
        }
    };
    for (const QString& root : roots)
    {
        consider(root);
    }
    if (!placementRoot.isEmpty())
    {
        consider(placementRoot);
    }
    return best;
}

QString SyncEngine::RelPathFor(const QString& path) const
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    QString root = RootOf(clean);
    if (root.isEmpty())
    {
        return FileNameOf(clean);
    }
    if (root.endsWith(u'/'))
    {
        root.chop(1);
    }
    return clean.section(u'/', root.count(u'/') + 1);
}

QString SyncEngine::PlacementPathFor(const QString& relPath) const
{
    if (placementRoot.isEmpty())
    {
        return {};
    }
    QString normalized = relPath;
    normalized.replace(u'\\', u'/');
    QStringList segments;
    const QStringList parts = normalized.split(u'/', Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        if (part == QLatin1String(".") || part == QLatin1String(".."))
        {
            continue;
        }
        const QString segment = CleanSegment(part);
        if (!segment.isEmpty())
        {
            segments.append(segment);
        }
    }
    if (segments.isEmpty())
    {
        return {};
    }
    if (segments.size() > MaxPathSegments)
    {
        const QString last = segments.constLast();
        segments = segments.mid(0, MaxPathSegments - 1);
        segments.append(last);
    }
    return QDir::cleanPath(placementRoot + u'/' + segments.join(u'/'));
}

void SyncEngine::IndexFiles()
{
    if (!bStarted || !bLibraryReady)
    {
        return;
    }

    struct Fresh
    {
        QString pathKey;
        QString path;
        qint64 size = 0;
        qint64 mtimeMs = 0;
        quint32 trackId = 0;
    };

    const qint64 now = NowMs();
    QSet<QString> seen;
    QList<Fresh> fresh;
    const QList<Track>& tracks = LibrarySubsystem::Get().GetTracks();
    for (const Track& track : tracks)
    {
        if (!track.HasLocalFile())
        {
            continue;
        }
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(track.file.path));
        const QString pathKey = Sync::PathKey(path);
        if (seen.contains(pathKey))
        {
            continue;
        }
        qint64 size = track.file.size;
        qint64 mtimeMs = track.file.modifiedMs;
        if (size <= 0 || mtimeMs <= 0)
        {
            const QFileInfo info(path);
            size = info.exists() ? info.size() : 0;
            mtimeMs = info.exists() ? ModifiedMs(info) : 0;
        }
        const auto known = state.files.find(pathKey);
        if (size <= 0)
        {
            if (known != state.files.end())
            {
                seen.insert(pathKey);
                goneSince.remove(pathKey);
            }
            DropCandidate(pathKey);
            continue;
        }
        seen.insert(pathKey);
        goneSince.remove(pathKey);
        if (known != state.files.end() && !known->sha.isEmpty() && known->size == size &&
            SameTime(known->mtimeMs, mtimeMs))
        {
            known->trackId = track.id.value;
            if (known->path != path)
            {
                known->path = path;
                ScheduleSave();
            }
            DropCandidate(pathKey);
            continue;
        }
        fresh.append({pathKey, path, size, mtimeMs, track.id.value});
    }

    QHash<quint32, QString> unseenByTrack;
    QHash<QString, QString> unseenByFingerprint;
    for (auto it = state.files.cbegin(); it != state.files.cend(); ++it)
    {
        if (seen.contains(it.key()) || it->sha.isEmpty())
        {
            continue;
        }
        if (it->trackId != 0)
        {
            unseenByTrack.insert(it->trackId, it.key());
        }
        unseenByFingerprint.insert(Fingerprint(it->size, it->mtimeMs, FileNameOf(it.key())), it.key());
    }

    for (const Fresh& file : std::as_const(fresh))
    {
        if (!state.files.contains(file.pathKey))
        {
            QString from;
            if (file.trackId != 0)
            {
                const QString byTrack = unseenByTrack.value(file.trackId);
                if (!byTrack.isEmpty())
                {
                    const Sync::LocalFile old = state.files.value(byTrack);
                    if (old.size == file.size && SameTime(old.mtimeMs, file.mtimeMs))
                    {
                        from = byTrack;
                    }
                }
            }
            if (from.isEmpty())
            {
                from = unseenByFingerprint.value(Fingerprint(file.size, file.mtimeMs, FileNameOf(file.pathKey)));
            }
            if (!from.isEmpty() && state.files.contains(from))
            {
                Sync::LocalFile moved = state.files.value(from);
                if (moved.trackId != 0)
                {
                    unseenByTrack.remove(moved.trackId);
                }
                unseenByFingerprint.remove(Fingerprint(moved.size, moved.mtimeMs, FileNameOf(from)));
                moved.path = file.path;
                moved.trackId = file.trackId;
                moved.size = file.size;
                moved.mtimeMs = file.mtimeMs;
                PutLocal(file.pathKey, moved);
                RemoveLocal(from);
                goneSince.remove(from);
                CloseServed(from);
                DropCandidate(file.pathKey);
                continue;
            }
            const auto parked = detached.find(file.pathKey);
            if (parked != detached.end() && !parked->sha.isEmpty() && parked->size == file.size &&
                SameTime(parked->mtimeMs, file.mtimeMs))
            {
                Sync::LocalFile local = *parked;
                local.path = file.path;
                local.trackId = file.trackId;
                detached.erase(parked);
                DropCandidate(file.pathKey);
                LinkNewFile(local);
                continue;
            }
        }

        Candidate& candidate = candidates[file.pathKey];
        if (candidate.observedMs == 0 || candidate.size != file.size || !SameTime(candidate.mtimeMs, file.mtimeMs))
        {
            if (candidate.cancel)
            {
                candidate.cancel->store(true);
                candidate.cancel.reset();
            }
            candidate.bHashing = false;
            candidate.failures = 0;
            candidate.retryAtMs = 0;
            candidate.observedMs = now;
            candidate.size = file.size;
            candidate.mtimeMs = file.mtimeMs;
        }
        candidate.path = file.path;
        candidate.trackId = file.trackId;
    }

    for (auto it = state.files.cbegin(); it != state.files.cend(); ++it)
    {
        if (!seen.contains(it.key()) && !goneSince.contains(it.key()))
        {
            goneSince.insert(it.key(), now);
        }
    }
    for (auto it = candidates.begin(); it != candidates.end();)
    {
        if (seen.contains(it.key()))
        {
            ++it;
            continue;
        }
        if (it->cancel)
        {
            it->cancel->store(true);
        }
        it = candidates.erase(it);
    }

    bStartupPass = false;
    ScheduleHashing();
    ProcessGone();
    ScheduleProgress();
}

bool SyncEngine::IsIndexIdle() const
{
    if (!bLibraryReady || bStartupPass)
    {
        return false;
    }
    const qint64 now = NowMs();
    for (const Candidate& candidate : candidates)
    {
        if (candidate.bHashing ||
            (candidate.failures < MaxHashFailures && IsStable(candidate.mtimeMs, candidate.observedMs, now)))
        {
            return false;
        }
    }
    return true;
}

void SyncEngine::ScheduleHashing()
{
    if (!bStarted)
    {
        return;
    }
    const qint64 now = NowMs();
    for (auto it = candidates.begin(); it != candidates.end() && activeHashes < MaxConcurrentHashes; ++it)
    {
        Candidate& candidate = *it;
        if (candidate.bHashing || candidate.retryAtMs > now || !IsStable(candidate.mtimeMs, candidate.observedMs, now))
        {
            continue;
        }
        candidate.bHashing = true;
        candidate.cancel = std::make_shared<std::atomic_bool>(false);
        ++activeHashes;
        const QString pathKey = it.key();
        const Candidate snapshot = candidate;
        pool.start([this, pathKey, snapshot]
        {
            QFileInfo info(snapshot.path);
            QByteArray sha;
            if (info.exists() && info.size() == snapshot.size && SameTime(ModifiedMs(info), snapshot.mtimeMs))
            {
                sha = HashFile(snapshot.path, snapshot.size, *snapshot.cancel);
                info.refresh();
            }
            Candidate result = snapshot;
            result.size = info.exists() ? info.size() : -1;
            result.mtimeMs = info.exists() ? ModifiedMs(info) : 0;
            QMetaObject::invokeMethod(this, [this, pathKey, result, sha]
            {
                OnHashed(pathKey, result, sha);
            }, Qt::QueuedConnection);
        });
    }
}

void SyncEngine::OnHashed(const QString& pathKey, const Candidate& result, const QByteArray& sha)
{
    activeHashes = std::max(0, activeHashes - 1);
    if (!bStarted)
    {
        return;
    }
    const auto it = candidates.find(pathKey);
    if (it == candidates.end() || it->cancel != result.cancel)
    {
        ScheduleHashing();
        return;
    }
    Candidate& candidate = *it;
    candidate.bHashing = false;
    const qint64 now = NowMs();
    if (result.size < 0 || sha.isEmpty())
    {
        if (result.size >= 0 && (result.size != candidate.size || !SameTime(result.mtimeMs, candidate.mtimeMs)))
        {
            candidate.size = result.size;
            candidate.mtimeMs = result.mtimeMs;
            candidate.observedMs = now;
            candidate.failures = 0;
            candidate.retryAtMs = 0;
        }
        else
        {
            ++candidate.failures;
            candidate.retryAtMs = now + (candidate.failures >= MaxHashFailures ? HashRetryMs : ShortHashRetryMs);
            if (candidate.failures == MaxHashFailures)
            {
                Kick(libraryTimer);
            }
        }
        ScheduleHashing();
        ScheduleProgress();
        return;
    }
    if (result.size != candidate.size || !SameTime(result.mtimeMs, candidate.mtimeMs))
    {
        candidate.size = result.size;
        candidate.mtimeMs = result.mtimeMs;
        candidate.observedMs = now;
        candidate.failures = 0;
        candidate.retryAtMs = 0;
        ScheduleHashing();
        ScheduleProgress();
        return;
    }

    Sync::LocalFile local;
    local.path = candidate.path;
    local.size = candidate.size;
    local.mtimeMs = candidate.mtimeMs;
    local.sha = sha;
    local.observedMs = candidate.observedMs;
    local.trackId = candidate.trackId;
    candidates.erase(it);

    const auto previousIt = state.files.constFind(pathKey);
    const Sync::LocalFile previous = previousIt != state.files.cend() ? *previousIt : Sync::LocalFile();
    if (previousIt == state.files.cend() || previous.uid.isEmpty())
    {
        LinkNewFile(local);
    }
    else
    {
        local.uid = previous.uid;
        const QString key = Sync::MakeKey(Sync::Kind::File, previous.uid);
        const auto entry = state.entries.constFind(key);
        if (previous.sha == sha)
        {
            PutLocal(pathKey, local);
        }
        else if (entry == state.entries.cend() || entry->record.bDeleted)
        {
            LinkNewFile(local);
        }
        else if (entry->record.file.sha256 == sha)
        {
            PutLocal(pathKey, local);
        }
        else if (previous.sha == entry->record.file.sha256)
        {
            Sync::Record record = entry->record;
            QList<QByteArray>& history = record.file.history;
            history.removeAll(sha);
            history.removeAll(record.file.sha256);
            history.prepend(record.file.sha256);
            while (history.size() > MaxHistory)
            {
                history.removeLast();
            }
            record.file.sha256 = sha;
            record.file.size = quint64(local.size);
            record.file.mtimeMs = local.mtimeMs;
            record.file.relPath = RelPathFor(local.path);
            const Sync::TrackInfo info = InfoOf(local.trackId);
            if (!info.title.isEmpty())
            {
                record.file.info = info;
            }
            PutLocal(pathKey, local);
            Commit(key, record);
            UpgradeCopies(key);
        }
        else
        {
            const Sync::Record theirs = entry->record;
            Sync::Record mine = theirs;
            QList<QByteArray>& history = mine.file.history;
            history.removeAll(sha);
            history.removeAll(previous.sha);
            history.removeAll(theirs.file.sha256);
            if (!previous.sha.isEmpty())
            {
                history.prepend(previous.sha);
            }
            history.prepend(theirs.file.sha256);
            while (history.size() > MaxHistory)
            {
                history.removeLast();
            }
            mine.file.sha256 = sha;
            mine.file.size = quint64(local.size);
            mine.file.mtimeMs = local.mtimeMs;
            mine.file.relPath = RelPathFor(local.path);
            const Sync::TrackInfo info = InfoOf(local.trackId);
            if (!info.title.isEmpty())
            {
                mine.file.info = info;
            }
            mine.vv = theirs.vv;
            mine.hlc = NextHlc();
            mine.changedBy = state.deviceId;
            PutLocal(pathKey, local);
            if (FindConflict(key))
            {
                Commit(key, mine);
            }
            else
            {
                AddConflict(Sync::ConflictType::Content, key, {},
                            theirs.changedBy == state.deviceId ? QString() : theirs.changedBy, mine, theirs);
            }
        }
    }

    RunNameChecks();
    ProcessGone();
    ScheduleHashing();
    ScheduleProgress();
    FlushNotifications();
}

void SyncEngine::LinkNewFile(Sync::LocalFile local)
{
    local.uid.clear();
    const QString pathKey = Sync::PathKey(local.path);

    QStringList alive = keysBySha.values(local.sha);
    if (!alive.isEmpty())
    {
        std::sort(alive.begin(), alive.end());
        QString key = alive.constFirst();
        for (const QString& candidate : std::as_const(alive))
        {
            if (needContent.contains(candidate))
            {
                key = candidate;
                break;
            }
        }
        local.uid = state.entries.value(key).record.uid;
        PutLocal(pathKey, local);
        Unpark(key);
        const Sync::Conflict* conflict = FindConflict(key);
        if (conflict && conflict->type == Sync::ConflictType::LocalDelete)
        {
            const QString id = conflict->id;
            RemoveConflict(id);
        }
        return;
    }

    QStringList old = keysByOldSha.values(local.sha);
    std::sort(old.begin(), old.end());
    old.erase(std::unique(old.begin(), old.end()), old.end());
    for (const QString& key : std::as_const(old))
    {
        const auto entry = state.entries.constFind(key);
        if (entry == state.entries.cend() || !entry->record.bDeleted || entry->record.file.sha256 != local.sha)
        {
            continue;
        }
        const Sync::Record before = entry->record;
        Sync::Record record = before;
        record.bDeleted = false;
        record.file.size = quint64(local.size);
        record.file.mtimeMs = local.mtimeMs;
        record.file.relPath = RelPathFor(local.path);
        const Sync::TrackInfo info = InfoOf(local.trackId);
        if (!info.title.isEmpty())
        {
            record.file.info = info;
        }
        if (record.file.onlineUid.isEmpty())
        {
            record.file.onlineUid = OnlineUidOf(local.trackId);
        }
        local.uid = record.uid;
        PutLocal(pathKey, local);
        Commit(key, record);
        AfterChange(key, before, {});
        return;
    }
    for (const QString& key : std::as_const(old))
    {
        const auto entry = state.entries.constFind(key);
        if (entry == state.entries.cend() || entry->record.bDeleted || !entry->record.file.history.contains(local.sha))
        {
            continue;
        }
        local.uid = entry->record.uid;
        PutLocal(pathKey, local);
        Unpark(key);
        if (HoldsCurrent(key))
        {
            UpgradeCopies(key);
        }
        return;
    }

    Sync::Record record;
    record.kind = Sync::Kind::File;
    record.uid = NewFileUid(local.sha);
    record.file.sha256 = local.sha;
    record.file.size = quint64(local.size);
    record.file.mtimeMs = local.mtimeMs;
    record.file.relPath = RelPathFor(local.path);
    record.file.info = InfoOf(local.trackId);
    record.file.onlineUid = OnlineUidOf(local.trackId);
    local.uid = record.uid;
    PutLocal(pathKey, local);
    Commit(record.Key(), record);
}

QString SyncEngine::NewFileUid(const QByteArray& sha) const
{
    QString uid = sha.isEmpty() ? QString() : Sync::FileUid(sha);
    while (uid.isEmpty() || state.entries.contains(Sync::MakeKey(Sync::Kind::File, uid)))
    {
        quint32 words[4];
        QRandomGenerator::system()->fillRange(words);
        uid = Sync::ToHex(QByteArray(reinterpret_cast<const char*>(words), sizeof(words)));
    }
    return uid;
}

Sync::TrackInfo SyncEngine::InfoOf(quint32 trackId) const
{
    Sync::TrackInfo info;
    if (trackId == 0)
    {
        return info;
    }
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{trackId});
    if (track)
    {
        info.title = track->tags.title;
        info.artist = track->tags.artist;
        info.album = track->tags.album;
        info.durationMs = track->tags.durationMs;
    }
    return info;
}

QString SyncEngine::OnlineUidOf(quint32 trackId) const
{
    if (trackId == 0)
    {
        return {};
    }
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{trackId});
    return track && track->IsOnline() ? Sync::OnlineUid(track->stream.pageUrl) : QString();
}

bool SyncEngine::IsCurrentTrackFile(const QString& pathKey) const
{
    const TrackId current = PlayerSubsystem::Get().GetCurrentTrack();
    if (!current.IsValid())
    {
        return false;
    }
    const Track* track = LibrarySubsystem::Get().FindTrack(current);
    return track && !track->file.path.isEmpty() && Sync::PathKey(track->file.path) == pathKey;
}

void SyncEngine::DetachCopy(const QString& pathKey)
{
    goneSince.remove(pathKey);
    CloseServed(pathKey);
    const auto it = state.files.constFind(pathKey);
    if (it == state.files.cend())
    {
        return;
    }
    const Sync::LocalFile local = *it;
    if (!local.sha.isEmpty())
    {
        detached.insert(pathKey, local);
    }
    QString key;
    if (!local.uid.isEmpty())
    {
        key = Sync::MakeKey(Sync::Kind::File, local.uid);
        const auto entry = state.entries.constFind(key);
        if (filesByUid.count(local.uid) <= 1 && entry != state.entries.cend() && !entry->record.bDeleted)
        {
            state.parked.insert(key);
        }
    }
    RemoveLocal(pathKey);
    if (!key.isEmpty())
    {
        RefreshNeed(key);
    }
    ScheduleSave();
}

void SyncEngine::ProcessGone()
{
    if (goneSince.isEmpty() || !IsIndexIdle())
    {
        return;
    }
    const qint64 now = NowMs();
    QStringList deletedKeys;
    QHash<QString, QString> pathByKey;
    const QStringList pathKeys = goneSince.keys();
    for (const QString& pathKey : pathKeys)
    {
        if (now - goneSince.value(pathKey) < GoneDelayMs)
        {
            continue;
        }
        const auto it = state.files.constFind(pathKey);
        if (it == state.files.cend())
        {
            goneSince.remove(pathKey);
            continue;
        }
        const Sync::LocalFile local = *it;
        if (QFileInfo::exists(local.path))
        {
            DetachCopy(pathKey);
            continue;
        }
        QString anchor = RootOf(local.path);
        if (anchor.isEmpty())
        {
            anchor = QFileInfo(local.path).absolutePath();
        }
        if (!QFileInfo(anchor).isDir())
        {
            DetachCopy(pathKey);
            continue;
        }
        goneSince.remove(pathKey);
        detached.remove(pathKey);
        CloseServed(pathKey);
        RemoveLocal(pathKey);
        if (local.uid.isEmpty() || filesByUid.contains(local.uid))
        {
            continue;
        }
        const QString key = Sync::MakeKey(Sync::Kind::File, local.uid);
        const auto entry = state.entries.constFind(key);
        if (entry == state.entries.cend() || entry->record.bDeleted || state.parked.contains(key))
        {
            continue;
        }
        if (!pathByKey.contains(key))
        {
            pathByKey.insert(key, local.path);
            deletedKeys.append(key);
        }
    }
    if (deletedKeys.isEmpty())
    {
        return;
    }
    if (deletedKeys.size() > MassDeleteThreshold)
    {
        for (const QString& key : std::as_const(deletedKeys))
        {
            if (!HasConflict(key))
            {
                const Sync::Record record = state.entries.value(key).record;
                AddConflict(Sync::ConflictType::LocalDelete, key, {}, {}, record, record, pathByKey.value(key));
            }
        }
    }
    else
    {
        for (const QString& key : std::as_const(deletedKeys))
        {
            TombstoneItem(key);
        }
    }
    AfterBatch();
}

void SyncEngine::DropCandidate(const QString& pathKey)
{
    const auto it = candidates.find(pathKey);
    if (it == candidates.end())
    {
        return;
    }
    if (it->cancel)
    {
        it->cancel->store(true);
    }
    candidates.erase(it);
    ScheduleProgress();
}

void SyncEngine::Unpark(const QString& key)
{
    if (state.parked.remove(key))
    {
        RefreshNeed(key);
        ScheduleSave();
    }
}

bool SyncEngine::IsUnchangedOnDisk(const Sync::LocalFile& local) const
{
    const QFileInfo info(local.path);
    return info.exists() && info.isFile() && info.size() == local.size && SameTime(ModifiedMs(info), local.mtimeMs);
}
