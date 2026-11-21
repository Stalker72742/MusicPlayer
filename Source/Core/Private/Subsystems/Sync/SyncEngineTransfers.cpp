//
// Created by Stalker7274 on 20.11.2026.
//

#include "SyncEngine.h"

#include "SyncSession.h"
#include "sync.qpb.h"

#include "Library/LibrarySubsystem.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace proto = soundlink::sync::v1;

namespace
{
    constexpr qint64 BlockSize = 256 * 1024;
    constexpr qsizetype MaxInflightBlocks = 16;
    constexpr int MaxDownloadsPerPeer = 2;
    constexpr int MaxLocalCopies = 2;
    constexpr qint64 MaxBlockLength = 1024 * 1024;
    constexpr qsizetype ReadChunk = 1024 * 1024;
    constexpr qint64 DiskReserveBytes = 64LL * 1024 * 1024;
    constexpr qint64 ServeBusyBytes = 32LL * 1024 * 1024;
    constexpr qint64 ServedIdleMs = 10000;
    constexpr qsizetype MaxServedFiles = 16;
    constexpr qint64 DownloadStallMs = 60000;
    constexpr int BusyRetryMs = 250;
    constexpr qint64 TempCleanupIntervalMs = 60LL * 60 * 1000;
    constexpr qint64 StalePartMs = 7LL * 24 * 60 * 60 * 1000;
    constexpr qint64 OrphanPartMs = 10LL * 60 * 1000;
    constexpr int MaxFailures = 10;
    constexpr qint64 BackoffBaseMs = 5000;
    constexpr qint64 BackoffMaxMs = 300000;
    constexpr int MaxNameSuffix = 999;

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

    qint64 ModifiedMs(const QFileInfo& info)
    {
        return info.lastModified().toMSecsSinceEpoch();
    }

    QString WithSuffix(const QString& path, const QString& suffix)
    {
        const qsizetype slash = path.lastIndexOf(u'/');
        const qsizetype dot = path.lastIndexOf(u'.');
        if (dot > slash + 1)
        {
            return path.left(dot) + suffix + path.mid(dot);
        }
        return path + suffix;
    }

    QString PartName(const QByteArray& sha)
    {
        return Sync::ToHex(sha) + QStringLiteral(".part");
    }

    QString ExistingAncestor(const QString& path)
    {
        QString current = QDir::cleanPath(path);
        while (!current.isEmpty())
        {
            if (QFileInfo(current).isDir())
            {
                return current;
            }
            const qsizetype slash = current.lastIndexOf(u'/');
            if (slash < 0)
            {
                break;
            }
            QString parent = current.left(slash);
            if (parent.size() == 2 && parent.at(1) == u':')
            {
                parent += u'/';
            }
            if (parent.isEmpty() || parent == current)
            {
                break;
            }
            current = parent;
        }
        return {};
    }

    bool MakeHiddenDir(const QString& path)
    {
        if (path.isEmpty() || !QDir().mkpath(path))
        {
            return false;
        }
#ifdef Q_OS_WIN
        const QString native = QDir::toNativeSeparators(path);
        const auto* name = reinterpret_cast<const wchar_t*>(native.utf16());
        const DWORD attributes = GetFileAttributesW(name);
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_HIDDEN))
        {
            SetFileAttributesW(name, attributes | FILE_ATTRIBUTE_HIDDEN);
        }
#endif
        return true;
    }

    QByteArray CopyAndHash(QFile& source, const QString& target, qint64 size, const std::atomic_bool& cancel)
    {
        QFile output(target);
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return {};
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        QByteArray buffer(ReadChunk, Qt::Uninitialized);
        qint64 total = 0;
        bool bOk = true;
        while (!cancel.load())
        {
            const qint64 read = source.read(buffer.data(), buffer.size());
            if (read < 0)
            {
                bOk = false;
                break;
            }
            if (read == 0)
            {
                break;
            }
            if (output.write(buffer.constData(), read) != read)
            {
                bOk = false;
                break;
            }
            hash.addData(QByteArrayView(buffer.constData(), read));
            total += read;
        }
        bOk = bOk && !cancel.load() && total == size && output.flush();
        output.close();
        if (!bOk || output.error() != QFileDevice::NoError)
        {
            output.remove();
            return {};
        }
        return hash.result();
    }
}

QString SyncEngine::TempDir() const
{
    if (placementRoot.isEmpty())
    {
        return {};
    }
    return QDir::cleanPath(placementRoot + QStringLiteral("/.soundlink-tmp"));
}

QString SyncEngine::PartPath(const QByteArray& sha) const
{
    const QString dir = TempDir();
    if (dir.isEmpty() || sha.isEmpty())
    {
        return {};
    }
    return dir + u'/' + PartName(sha);
}

void SyncEngine::CloseServed(const QString& pathKey)
{
    for (auto it = served.begin(); it != served.end();)
    {
        if ((*it)->pathKey == pathKey)
        {
            it = served.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SyncEngine::Backoff(const QString& key)
{
    const int count = ++failures[key];
    const qint64 delay = std::min(BackoffBaseMs << std::min(count - 1, 10), BackoffMaxMs);
    retryAtMs.insert(key, NowMs() + delay);
    if (count == MaxFailures)
    {
        qInfo() << "Sync: giving up on" << key << "after" << count << "failed attempts";
    }
    ScheduleProgress();
}

void SyncEngine::SchedulePump()
{
    Kick(pumpTimer);
}

void SyncEngine::StartDownload(const QString& key, Link& link)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend() || downloads.contains(key))
    {
        return;
    }
    auto download = std::make_shared<Download>();
    download->key = key;
    download->sha = entry->record.file.sha256;
    download->size = qint64(entry->record.file.size);
    download->peerId = link.peerId;
    download->partPath = PartPath(download->sha);
    download->cancel = std::make_shared<std::atomic_bool>(false);
    download->lastActivityMs = NowMs();
    if (download->partPath.isEmpty() || !MakeHiddenDir(TempDir()))
    {
        Backoff(key);
        return;
    }
    download->file = std::make_unique<QFile>(download->partPath);
    if (!download->file->open(QIODevice::ReadWrite))
    {
        qWarning() << "Sync: cannot open" << download->partPath << download->file->errorString();
        Backoff(key);
        return;
    }
    qint64 have = download->file->size();
    if (have > download->size)
    {
        download->file->resize(0);
        have = 0;
    }
    download->received = have;
    download->requested = have;
    downloads.insert(key, download);
    ScheduleProgress();
    if (have == download->size)
    {
        download->file.reset();
        VerifyDownload(key);
        return;
    }
    RequestBlocks(*download, link);
}

void SyncEngine::RequestBlocks(Download& download, Link& link)
{
    if (!link.session || !link.session->IsEstablished() || !download.file || download.bBusy || download.bReady)
    {
        return;
    }
    QList<proto::Frame> frames;
    while (download.inflight.size() < MaxInflightBlocks && download.requested < download.size)
    {
        const qint64 length = std::min(BlockSize, download.size - download.requested);
        quint32 id = ++nextRequestId;
        if (id == 0)
        {
            id = ++nextRequestId;
        }
        proto::BlockRequest request;
        request.setId_proto(id);
        request.setSha256(download.sha);
        request.setOffset(quint64(download.requested));
        request.setLength(quint32(length));
        proto::Frame frame;
        frame.setBlockRequest(request);
        frames.append(frame);
        download.inflight.insert(id, download.requested);
        link.blockRequests.insert(id, download.key);
        download.requested += length;
    }
    if (frames.isEmpty())
    {
        return;
    }
    download.lastActivityMs = NowMs();
    const QPointer<SyncSession> session = link.session;
    for (const proto::Frame& frame : std::as_const(frames))
    {
        if (!session)
        {
            break;
        }
        session->SendFrame(frame);
    }
}

void SyncEngine::OnBlockData(const QString& peerId, const proto::BlockData& data)
{
    const auto link = links.find(peerId);
    if (link == links.end())
    {
        return;
    }
    const quint32 id = data.id_proto();
    const QString key = link->blockRequests.take(id);
    if (key.isEmpty())
    {
        return;
    }
    const auto found = downloads.constFind(key);
    if (found == downloads.cend())
    {
        return;
    }
    const std::shared_ptr<Download> download = *found;
    if (download->peerId != peerId || !download->inflight.contains(id))
    {
        return;
    }
    const qint64 offset = download->inflight.take(id);
    download->lastActivityMs = NowMs();

    const auto restart = [&download, &link]
    {
        for (auto it = download->inflight.cbegin(); it != download->inflight.cend(); ++it)
        {
            link->blockRequests.remove(it.key());
        }
        download->inflight.clear();
        download->requested = download->received;
    };
    const auto dropPeer = [this, &key, &peerId, &download]
    {
        const auto entry = state.entries.find(key);
        if (entry != state.entries.end() && entry->record.file.sha256 == download->sha && entry->haveBy.remove(peerId))
        {
            ScheduleSave();
        }
        AbortDownload(key, true);
        SchedulePump();
    };
    const auto fail = [this, &key]
    {
        AbortDownload(key, true);
        Backoff(key);
    };

    const QString error = data.error();
    if (!error.isEmpty())
    {
        if (error == QLatin1String("busy"))
        {
            restart();
            const std::shared_ptr<std::atomic_bool> token = download->cancel;
            QTimer::singleShot(BusyRetryMs, this, [this, key, peerId, token]
            {
                const auto current = downloads.constFind(key);
                if (!bStarted || current == downloads.cend() || (*current)->cancel != token || (*current)->bBusy ||
                    (*current)->bReady)
                {
                    return;
                }
                const auto retryLink = links.find(peerId);
                if (retryLink != links.end())
                {
                    RequestBlocks(**current, *retryLink);
                }
            });
        }
        else if (error == QLatin1String("not_found") || error == QLatin1String("changed"))
        {
            dropPeer();
        }
        else
        {
            fail();
        }
        return;
    }
    if (qint64(data.offset()) != offset)
    {
        fail();
        return;
    }
    if (offset != download->received)
    {
        restart();
        RequestBlocks(*download, *link);
        return;
    }
    const QByteArray bytes = data.data();
    const qint64 expected = std::min(BlockSize, download->size - offset);
    if (bytes.size() != expected)
    {
        if (data.eof() && bytes.size() < expected)
        {
            dropPeer();
        }
        else
        {
            fail();
        }
        return;
    }
    if (!download->file || !download->file->seek(offset) || download->file->write(bytes) != bytes.size())
    {
        qWarning() << "Sync: cannot write" << download->partPath;
        fail();
        return;
    }
    download->received += bytes.size();
    ScheduleProgress();
    if (download->received >= download->size)
    {
        VerifyDownload(key);
        return;
    }
    if (data.eof())
    {
        dropPeer();
        return;
    }
    RequestBlocks(*download, *link);
}

void SyncEngine::AbortDownload(const QString& key, bool bKeepPart)
{
    const auto it = downloads.find(key);
    if (it == downloads.end())
    {
        return;
    }
    const std::shared_ptr<Download> download = *it;
    downloads.erase(it);
    if (download->cancel)
    {
        download->cancel->store(true);
    }
    if (!download->peerId.isEmpty())
    {
        const auto link = links.find(download->peerId);
        if (link != links.end())
        {
            for (auto request = download->inflight.cbegin(); request != download->inflight.cend(); ++request)
            {
                link->blockRequests.remove(request.key());
            }
        }
    }
    download->inflight.clear();
    download->file.reset();
    if (!bKeepPart && !download->bBusy && !download->partPath.isEmpty())
    {
        QFile::remove(download->partPath);
    }
    ScheduleProgress();
    SchedulePump();
}

void SyncEngine::VerifyDownload(const QString& key)
{
    const auto it = downloads.constFind(key);
    if (it == downloads.cend() || (*it)->bBusy)
    {
        return;
    }
    const std::shared_ptr<Download> download = *it;
    download->file.reset();
    download->bBusy = true;
    const std::shared_ptr<std::atomic_bool> cancel = download->cancel;
    const QString partPath = download->partPath;
    const QByteArray sha = download->sha;
    const qint64 size = download->size;
    pool.start([this, key, cancel, partPath, sha, size]
    {
        const bool bMatch = HashFile(partPath, size, *cancel) == sha;
        QMetaObject::invokeMethod(this, [this, key, cancel, bMatch]
        {
            OnContentChecked(key, cancel, bMatch);
        }, Qt::QueuedConnection);
    });
}

void SyncEngine::StartLocalCopy(const QString& key, const QString& sourcePath)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend() || downloads.contains(key))
    {
        return;
    }
    auto download = std::make_shared<Download>();
    download->key = key;
    download->sha = entry->record.file.sha256;
    download->size = qint64(entry->record.file.size);
    download->partPath = PartPath(download->sha);
    download->cancel = std::make_shared<std::atomic_bool>(false);
    download->lastActivityMs = NowMs();
    download->bBusy = true;
    if (download->partPath.isEmpty() || !MakeHiddenDir(TempDir()))
    {
        Backoff(key);
        return;
    }
    downloads.insert(key, download);
    ScheduleProgress();
    const std::shared_ptr<std::atomic_bool> cancel = download->cancel;
    const QString partPath = download->partPath;
    const QByteArray sha = download->sha;
    const qint64 size = download->size;
    pool.start([this, key, cancel, sourcePath, partPath, sha, size]
    {
        bool bMatch = false;
        QFile source;
        if (OpenForReading(source, sourcePath) && source.size() == size)
        {
            bMatch = CopyAndHash(source, partPath, size, *cancel) == sha;
        }
        QMetaObject::invokeMethod(this, [this, key, cancel, bMatch]
        {
            OnContentChecked(key, cancel, bMatch);
        }, Qt::QueuedConnection);
    });
}

void SyncEngine::OnContentChecked(const QString& key, const std::shared_ptr<std::atomic_bool>& cancel, bool bMatch)
{
    const auto it = downloads.constFind(key);
    if (!bStarted || it == downloads.cend() || (*it)->cancel != cancel)
    {
        return;
    }
    const std::shared_ptr<Download> download = *it;
    download->bBusy = false;
    if (!bMatch)
    {
        qWarning() << "Sync: received content of" << key << "does not match its checksum";
        AbortDownload(key, false);
        Backoff(key);
        return;
    }
    download->bReady = true;
    download->received = download->size;
    Finalize(key);
    SchedulePump();
    ScheduleProgress();
}

void SyncEngine::PumpDownloads()
{
    if (!bStarted || placementRoot.isEmpty())
    {
        return;
    }
    const qint64 now = NowMs();

    const QStringList current = downloads.keys();
    for (const QString& key : current)
    {
        const auto it = downloads.constFind(key);
        if (it != downloads.cend() && (*it)->bReady && !(*it)->bBusy && !deferred.contains(key) &&
            retryAtMs.value(key, 0) <= now)
        {
            Finalize(key);
        }
    }

    QSet<QByteArray> activeShas;
    QHash<QString, int> perPeer;
    int localCopies = 0;
    qint64 reserved = 0;
    for (const std::shared_ptr<Download>& download : std::as_const(downloads))
    {
        activeShas.insert(download->sha);
        if (download->peerId.isEmpty())
        {
            if (!download->bReady)
            {
                ++localCopies;
            }
        }
        else if (download->file)
        {
            ++perPeer[download->peerId];
        }
        if (!download->bReady)
        {
            reserved += std::max<qint64>(0, download->size - download->received);
        }
    }

    struct Want
    {
        qint64 size = 0;
        QString key;
    };
    QList<Want> wants;
    for (const QString& key : std::as_const(needContent))
    {
        if (downloads.contains(key) || failures.value(key, 0) >= MaxFailures || retryAtMs.value(key, 0) > now)
        {
            continue;
        }
        const auto entry = state.entries.constFind(key);
        if (entry != state.entries.cend())
        {
            wants.append({qint64(entry->record.file.size), key});
        }
    }
    std::sort(wants.begin(), wants.end(), [](const Want& a, const Want& b)
    {
        return a.size != b.size ? a.size < b.size : a.key < b.key;
    });

    const QString tempDir = TempDir();
    QHash<QString, qint64> parts;
    if (!wants.isEmpty())
    {
        QDirIterator iterator(tempDir, {QStringLiteral("*.part")}, QDir::Files | QDir::Hidden);
        while (iterator.hasNext())
        {
            const QFileInfo info = iterator.nextFileInfo();
            parts.insert(info.fileName().toLower(), info.size());
        }
    }

    QString blocked;
    qint64 available = 0;
    bool bSpaceKnown = false;
    const auto hasSpace = [&](qint64 need)
    {
        if (!bSpaceKnown)
        {
            const QString base = ExistingAncestor(tempDir);
            available = base.isEmpty() ? -1 : QStorageInfo(base).bytesAvailable();
            bSpaceKnown = true;
        }
        return available < 0 || need + reserved + DiskReserveBytes <= available;
    };

    for (const Want& want : std::as_const(wants))
    {
        const auto entry = state.entries.constFind(want.key);
        if (entry == state.entries.cend() || downloads.contains(want.key) || !needContent.contains(want.key))
        {
            continue;
        }
        const QByteArray sha = entry->record.file.sha256;
        const qint64 size = qint64(entry->record.file.size);
        const QSet<QString> haveBy = entry->haveBy;
        if (sha.isEmpty() || activeShas.contains(sha))
        {
            continue;
        }
        const qint64 partSize = parts.value(PartName(sha), -1);
        if (partSize == size)
        {
            auto download = std::make_shared<Download>();
            download->key = want.key;
            download->sha = sha;
            download->size = size;
            download->partPath = PartPath(sha);
            download->cancel = std::make_shared<std::atomic_bool>(false);
            download->lastActivityMs = now;
            download->received = size;
            download->requested = size;
            downloads.insert(want.key, download);
            activeShas.insert(sha);
            VerifyDownload(want.key);
            continue;
        }

        QStringList holders = localBySha.values(sha);
        std::sort(holders.begin(), holders.end());
        QString localSource;
        for (const QString& pathKey : std::as_const(holders))
        {
            const auto local = state.files.constFind(pathKey);
            if (local != state.files.cend() && IsUnchangedOnDisk(*local))
            {
                localSource = local->path;
                break;
            }
        }
        if (!localSource.isEmpty())
        {
            if (localCopies >= MaxLocalCopies)
            {
                continue;
            }
            if (!hasSpace(size))
            {
                blocked = QStringLiteral("disk_full");
                break;
            }
            StartLocalCopy(want.key, localSource);
            if (downloads.contains(want.key))
            {
                ++localCopies;
                activeShas.insert(sha);
                reserved += size;
            }
            continue;
        }

        QString bestPeer;
        int bestLoad = 0;
        for (const QString& peerId : haveBy)
        {
            if (peerId == state.deviceId)
            {
                continue;
            }
            const auto link = links.constFind(peerId);
            if (link == links.cend() || !link->session || !link->session->IsEstablished())
            {
                continue;
            }
            const int load = perPeer.value(peerId, 0);
            if (load >= MaxDownloadsPerPeer)
            {
                continue;
            }
            if (bestPeer.isEmpty() || load < bestLoad || (load == bestLoad && peerId < bestPeer))
            {
                bestPeer = peerId;
                bestLoad = load;
            }
        }
        if (bestPeer.isEmpty())
        {
            continue;
        }
        const qint64 missing = size - std::max<qint64>(0, std::min(partSize, size));
        if (!hasSpace(missing))
        {
            blocked = QStringLiteral("disk_full");
            break;
        }
        StartDownload(want.key, *links.find(bestPeer));
        if (downloads.contains(want.key))
        {
            ++perPeer[bestPeer];
            activeShas.insert(sha);
            reserved += missing;
        }
    }

    if (blocked != blockedReason)
    {
        if (!blocked.isEmpty())
        {
            qWarning() << "Sync: not enough free space for downloads in" << tempDir;
        }
        blockedReason = blocked;
        blockedSinceMs = blocked.isEmpty() ? 0 : now;
        ScheduleProgress();
    }
}

bool SyncEngine::Finalize(const QString& key)
{
    const auto found = downloads.constFind(key);
    if (found == downloads.cend() || !(*found)->bReady || (*found)->bBusy)
    {
        return true;
    }
    const std::shared_ptr<Download> download = *found;
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend() || entry->record.kind != Sync::Kind::File || entry->record.bDeleted ||
        entry->record.file.sha256 != download->sha || conflictedKeys.contains(key))
    {
        AbortDownload(key, keysBySha.contains(download->sha));
        return true;
    }
    const Sync::Record record = entry->record;

    QStringList copies = filesByUid.values(record.uid);
    std::sort(copies.begin(), copies.end());
    bool bHoldsCurrent = false;
    QStringList outdated;
    for (const QString& pathKey : std::as_const(copies))
    {
        const auto local = state.files.constFind(pathKey);
        if (local == state.files.cend() || local->sha.isEmpty())
        {
            continue;
        }
        if (local->sha == record.file.sha256)
        {
            bHoldsCurrent = true;
        }
        else
        {
            outdated.append(pathKey);
        }
    }
    if (outdated.isEmpty() && (bHoldsCurrent || !needContent.contains(key)))
    {
        AbortDownload(key, keysBySha.contains(download->sha));
        return true;
    }

    downloads.remove(key);
    bool bDeferred = false;
    bool bFailed = false;
    bool bAnyDone = false;
    bool bPartUsed = false;
    if (!outdated.isEmpty())
    {
        for (qsizetype i = 0; i < outdated.size(); ++i)
        {
            const bool bMove = i == outdated.size() - 1 && !bDeferred && !bFailed;
            switch (ReplaceCopy(outdated.at(i), download->partPath, bMove, record))
            {
            case ReplaceResult::Done:
                bAnyDone = true;
                bPartUsed = bPartUsed || bMove;
                break;
            case ReplaceResult::Deferred:
                bDeferred = true;
                break;
            case ReplaceResult::Skipped:
                break;
            case ReplaceResult::Failed:
                bFailed = true;
                break;
            }
        }
    }
    else
    {
        QString target = state.placements.value(key);
        if (target.isEmpty() || RootOf(target).isEmpty())
        {
            target = PlacementPathFor(record.file.relPath);
        }
        QString path = target;
        if (!path.isEmpty() && QFileInfo::exists(path))
        {
            const auto holder = state.files.constFind(Sync::PathKey(path));
            if (holder != state.files.cend() && !holder->uid.isEmpty())
            {
                const QString holderKey = Sync::MakeKey(Sync::Kind::File, holder->uid);
                const auto holderEntry = state.entries.constFind(holderKey);
                bDeferred = holderEntry != state.entries.cend() && holderEntry->record.bDeleted &&
                            deferred.contains(holderKey);
            }
            for (int n = 2; !bDeferred && QFileInfo::exists(path); ++n)
            {
                path = n <= MaxNameSuffix ? WithSuffix(target, QStringLiteral(" (%1)").arg(n)) : QString();
                if (path.isEmpty())
                {
                    break;
                }
            }
        }
        if (!bDeferred)
        {
            if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath()) ||
                !QFile::rename(download->partPath, path))
            {
                qWarning() << "Sync: cannot place" << key << "at" << (path.isEmpty() ? target : path);
                bFailed = true;
            }
            else
            {
                bPartUsed = true;
                bAnyDone = true;
                state.placements.remove(key);
                AdoptPlaced(path, record);
            }
        }
    }

    if (bDeferred)
    {
        downloads.insert(key, download);
        deferred.insert(key);
        ScheduleProgress();
        return false;
    }
    if (bFailed)
    {
        downloads.insert(key, download);
        Backoff(key);
        if (failures.value(key, 0) >= MaxFailures)
        {
            AbortDownload(key, false);
        }
        return false;
    }
    if (!bAnyDone)
    {
        downloads.insert(key, download);
        Backoff(key);
        return false;
    }
    if (!bPartUsed)
    {
        QFile::remove(download->partPath);
    }
    failures.remove(key);
    retryAtMs.remove(key);
    deferred.remove(key);
    ScheduleSave();
    ScheduleProgress();
    SchedulePump();
    return true;
}

SyncEngine::ReplaceResult SyncEngine::ReplaceCopy(const QString& pathKey, const QString& source, bool bMove,
                                                  const Sync::Record& record)
{
    const auto it = state.files.constFind(pathKey);
    if (it == state.files.cend())
    {
        return ReplaceResult::Skipped;
    }
    const Sync::LocalFile local = *it;
    if (IsCurrentTrackFile(pathKey))
    {
        deferred.insert(record.Key());
        return ReplaceResult::Deferred;
    }
    if (!IsUnchangedOnDisk(local))
    {
        return ReplaceResult::Skipped;
    }
    KeepOldContent(local);
    CloseServed(pathKey);
    QString trashed;
    if (!QFile::moveToTrash(local.path, &trashed))
    {
        qWarning() << "Sync: cannot move" << local.path << "to the Recycle Bin";
        return ReplaceResult::Failed;
    }
    const bool bPlaced = bMove ? QFile::rename(source, local.path) : QFile::copy(source, local.path);
    if (!bPlaced)
    {
        qWarning() << "Sync: cannot write" << local.path;
        if (!trashed.isEmpty())
        {
            QFile::rename(trashed, local.path);
        }
        return ReplaceResult::Failed;
    }
    AdoptPlaced(local.path, record);
    return ReplaceResult::Done;
}

void SyncEngine::AdoptPlaced(const QString& path, const Sync::Record& record)
{
    if (record.file.mtimeMs > 0)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadWrite | QIODevice::ExistingOnly))
        {
            file.setFileTime(QDateTime::fromMSecsSinceEpoch(record.file.mtimeMs, QTimeZone::UTC),
                             QFileDevice::FileModificationTime);
        }
    }
    const QFileInfo info(path);
    Sync::LocalFile local;
    local.path = QDir::cleanPath(info.absoluteFilePath());
    const QString pathKey = Sync::PathKey(local.path);
    local.size = info.size();
    local.mtimeMs = ModifiedMs(info);
    local.sha = record.file.sha256;
    local.uid = record.uid;
    local.observedMs = NowMs();
    local.trackId = state.files.value(pathKey).trackId;
    detached.remove(pathKey);
    goneSince.remove(pathKey);
    DropCandidate(pathKey);
    PutLocal(pathKey, local);

    if (record.file.onlineUid.isEmpty())
    {
        return;
    }
    const auto online = state.entries.constFind(Sync::MakeKey(Sync::Kind::Online, record.file.onlineUid));
    if (online == state.entries.cend() || online->record.bDeleted)
    {
        return;
    }
    LibrarySubsystem& library = LibrarySubsystem::Get();
    const TrackId id = library.FindOnlineTrack(online->record.online.pageUrl);
    const Track* track = id.IsValid() ? library.FindTrack(id) : nullptr;
    if (!track || track->HasLocalFile())
    {
        return;
    }
    const TrackId owner = library.FindTrackByPath(local.path);
    if (!owner.IsValid() || owner == id)
    {
        library.SetLocalFile(id, local.path);
    }
}

void SyncEngine::UpgradeCopies(const QString& key)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend() || entry->record.kind != Sync::Kind::File || entry->record.bDeleted ||
        conflictedKeys.contains(key) || downloads.contains(key) || placementRoot.isEmpty())
    {
        return;
    }
    const QByteArray sha = entry->record.file.sha256;
    QStringList copies = filesByUid.values(entry->record.uid);
    std::sort(copies.begin(), copies.end());
    QString source;
    bool bOutdated = false;
    for (const QString& pathKey : std::as_const(copies))
    {
        const auto local = state.files.constFind(pathKey);
        if (local == state.files.cend() || local->sha.isEmpty())
        {
            continue;
        }
        if (local->sha != sha)
        {
            bOutdated = true;
        }
        else if (source.isEmpty() && IsUnchangedOnDisk(*local))
        {
            source = local->path;
        }
    }
    if (!bOutdated || source.isEmpty())
    {
        return;
    }
    for (const std::shared_ptr<Download>& download : std::as_const(downloads))
    {
        if (download->sha == sha)
        {
            deferred.insert(key);
            ScheduleProgress();
            return;
        }
    }
    StartLocalCopy(key, source);
}

void SyncEngine::KeepOldContent(const Sync::LocalFile& local)
{
    if (local.sha.isEmpty() || placementRoot.isEmpty())
    {
        return;
    }
    bool bWanted = false;
    const QStringList keys = keysBySha.values(local.sha);
    for (const QString& key : keys)
    {
        if (needContent.contains(key))
        {
            bWanted = true;
            break;
        }
    }
    if (!bWanted)
    {
        return;
    }
    const QString pathKey = Sync::PathKey(local.path);
    const QStringList holders = localBySha.values(local.sha);
    for (const QString& holder : holders)
    {
        if (holder != pathKey)
        {
            return;
        }
    }
    for (const std::shared_ptr<Download>& download : std::as_const(downloads))
    {
        if (download->sha == local.sha)
        {
            return;
        }
    }
    const QString partPath = PartPath(local.sha);
    const QFileInfo part(partPath);
    if ((part.exists() && part.size() == local.size) || !MakeHiddenDir(TempDir()))
    {
        return;
    }
    QFile source;
    if (!OpenForReading(source, local.path) || source.size() != local.size)
    {
        return;
    }
    const std::atomic_bool never{false};
    if (CopyAndHash(source, partPath, local.size, never) != local.sha)
    {
        QFile::remove(partPath);
    }
}

void SyncEngine::ApplyFileTombstone(const QString& key, const QString& peerId)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend())
    {
        return;
    }
    QStringList copies = filesByUid.values(entry->record.uid);
    std::sort(copies.begin(), copies.end());
    LibrarySubsystem& library = LibrarySubsystem::Get();
    bool bPending = false;
    bool bFailed = false;
    int count = 0;
    for (const QString& pathKey : std::as_const(copies))
    {
        const auto it = state.files.constFind(pathKey);
        if (it == state.files.cend())
        {
            continue;
        }
        const Sync::LocalFile local = *it;
        if (IsCurrentTrackFile(pathKey))
        {
            bPending = true;
            continue;
        }
        if (!IsUnchangedOnDisk(local))
        {
            continue;
        }
        KeepOldContent(local);
        CloseServed(pathKey);
        if (!QFile::moveToTrash(local.path))
        {
            qWarning() << "Sync: cannot move" << local.path << "to the Recycle Bin";
            bFailed = true;
            bPending = true;
            continue;
        }
        const TrackId id = library.FindTrackByPath(local.path);
        const Track* track = id.IsValid() ? library.FindTrack(id) : nullptr;
        if (track && track->IsOnline())
        {
            library.SetLocalFile(id, {});
        }
        else if (track)
        {
            library.MarkFileGone(id);
        }
        goneSince.remove(pathKey);
        detached.remove(pathKey);
        RemoveLocal(pathKey);
        ++count;
    }
    if (bFailed)
    {
        Backoff(key);
    }
    if (bPending)
    {
        deferred.insert(key);
    }
    if (count > 0 && !peerId.isEmpty())
    {
        trashedBy[peerId] += count;
    }
    ScheduleProgress();
}

void SyncEngine::RetryDeferred()
{
    if (!bStarted || deferred.isEmpty())
    {
        return;
    }
    const qint64 now = NowMs();
    QStringList keys = deferred.values();
    std::sort(keys.begin(), keys.end());
    QStringList tombstones;
    QStringList others;
    for (const QString& key : std::as_const(keys))
    {
        if (retryAtMs.value(key, 0) > now)
        {
            continue;
        }
        const auto entry = state.entries.constFind(key);
        if (entry == state.entries.cend() || entry->record.kind != Sync::Kind::File ||
            failures.value(key, 0) >= MaxFailures)
        {
            deferred.remove(key);
            continue;
        }
        (entry->record.bDeleted ? tombstones : others).append(key);
    }
    for (const QString& key : std::as_const(tombstones))
    {
        deferred.remove(key);
        const QString changedBy = state.entries.value(key).record.changedBy;
        ApplyFileTombstone(key, state.peers.contains(changedBy) ? changedBy : QString());
    }
    for (const QString& key : std::as_const(others))
    {
        deferred.remove(key);
        if (downloads.contains(key))
        {
            Finalize(key);
        }
        else
        {
            UpgradeCopies(key);
        }
    }
    FlushNotifications();
    SchedulePump();
    ScheduleProgress();
}

void SyncEngine::ServeBlock(const QString& peerId, const proto::BlockRequest& request)
{
    const auto link = links.constFind(peerId);
    if (link == links.cend() || !link->session || !link->session->IsEstablished())
    {
        return;
    }
    const QPointer<SyncSession> session = link->session;
    proto::BlockData reply;
    reply.setId_proto(request.id_proto());
    reply.setOffset(request.offset());
    const auto send = [&session, &reply](const QString& error)
    {
        if (!error.isEmpty())
        {
            reply.setError(error);
        }
        proto::Frame frame;
        frame.setBlockData(reply);
        if (session)
        {
            session->SendFrame(frame);
        }
    };

    if (session->PendingBytes() > ServeBusyBytes)
    {
        send(QStringLiteral("busy"));
        return;
    }
    const QByteArray sha = request.sha256();
    if (sha.size() != 32)
    {
        send(QStringLiteral("not_found"));
        return;
    }

    const qint64 now = NowMs();
    std::shared_ptr<ServedFile> servedFile;
    const auto cached = served.constFind(sha);
    if (cached != served.cend())
    {
        const auto local = state.files.constFind((*cached)->pathKey);
        if (local != state.files.cend() && local->sha == sha && IsUnchangedOnDisk(*local))
        {
            servedFile = *cached;
        }
        else
        {
            served.remove(sha);
        }
    }
    bool bChanged = false;
    bool bIo = false;
    if (!servedFile)
    {
        QStringList holders = localBySha.values(sha);
        std::sort(holders.begin(), holders.end());
        for (const QString& pathKey : std::as_const(holders))
        {
            const auto local = state.files.constFind(pathKey);
            if (local == state.files.cend())
            {
                continue;
            }
            if (!IsUnchangedOnDisk(*local))
            {
                bChanged = true;
                continue;
            }
            auto file = std::make_unique<QFile>();
            if (!OpenForReading(*file, local->path))
            {
                bIo = true;
                continue;
            }
            if (file->size() != local->size)
            {
                bChanged = true;
                continue;
            }
            servedFile = std::make_shared<ServedFile>();
            servedFile->file = std::move(file);
            servedFile->pathKey = pathKey;
            served.insert(sha, servedFile);
            break;
        }
    }
    if (!servedFile)
    {
        send(bIo ? QStringLiteral("io") : bChanged ? QStringLiteral("changed") : QStringLiteral("not_found"));
        return;
    }
    servedFile->lastUsedMs = now;
    while (served.size() > MaxServedFiles)
    {
        auto oldest = served.end();
        for (auto it = served.begin(); it != served.end(); ++it)
        {
            if (it.value() != servedFile && (oldest == served.end() || it.value()->lastUsedMs < oldest.value()->lastUsedMs))
            {
                oldest = it;
            }
        }
        if (oldest == served.end())
        {
            break;
        }
        served.erase(oldest);
    }

    QFile& file = *servedFile->file;
    const qint64 size = file.size();
    const quint64 offset = request.offset();
    if (offset > quint64(size))
    {
        send(QStringLiteral("io"));
        return;
    }
    const qint64 length = std::min({qint64(request.length()), MaxBlockLength, size - qint64(offset)});
    if (!file.seek(qint64(offset)))
    {
        served.remove(sha);
        send(QStringLiteral("io"));
        return;
    }
    const QByteArray bytes = file.read(length);
    if (bytes.size() != length)
    {
        served.remove(sha);
        send(QStringLiteral("changed"));
        return;
    }
    reply.setData(bytes);
    reply.setEof(qint64(offset) + length >= size);
    send({});
}

void SyncEngine::CleanTemp()
{
    lastTempCleanupMs = NowMs();
    const QString dir = TempDir();
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
    {
        return;
    }
    QSet<QString> referenced;
    for (const std::shared_ptr<Download>& download : std::as_const(downloads))
    {
        referenced.insert(Sync::PathKey(download->partPath));
    }
    QSet<QString> wanted;
    for (const QString& key : std::as_const(needContent))
    {
        const auto entry = state.entries.constFind(key);
        if (entry != state.entries.cend())
        {
            wanted.insert(PartName(entry->record.file.sha256));
        }
    }
    QDirIterator iterator(dir, QDir::Files | QDir::Hidden | QDir::System);
    while (iterator.hasNext())
    {
        const QFileInfo info = iterator.nextFileInfo();
        const QString path = info.absoluteFilePath();
        if (referenced.contains(Sync::PathKey(path)))
        {
            continue;
        }
        const qint64 limit = wanted.contains(info.fileName().toLower()) ? StalePartMs : OrphanPartMs;
        if (lastTempCleanupMs - ModifiedMs(info) > limit)
        {
            QFile::remove(path);
        }
    }
}

void SyncEngine::Housekeeping()
{
    if (!bStarted)
    {
        return;
    }
    UpdatePlacementRoot(LibraryRoots());
    ScheduleHashing();
    ProcessGone();
    RunNameChecks();

    const qint64 now = NowMs();
    for (auto it = served.begin(); it != served.end();)
    {
        if (now - (*it)->lastUsedMs > ServedIdleMs)
        {
            it = served.erase(it);
        }
        else
        {
            ++it;
        }
    }

    QStringList stalled;
    bool bReadyWaiting = false;
    for (auto it = downloads.cbegin(); it != downloads.cend(); ++it)
    {
        const Download& download = **it;
        bReadyWaiting = bReadyWaiting || download.bReady;
        if (!download.peerId.isEmpty() && !download.bBusy && !download.bReady &&
            now - download.lastActivityMs > DownloadStallMs)
        {
            stalled.append(it.key());
        }
    }
    for (const QString& key : std::as_const(stalled))
    {
        qInfo() << "Sync: download of" << key << "stalled";
        AbortDownload(key, true);
        Backoff(key);
    }

    if (lastTempCleanupMs == 0 || now - lastTempCleanupMs > TempCleanupIntervalMs)
    {
        CleanTemp();
    }
    if (!deferred.isEmpty())
    {
        RetryDeferred();
    }
    if (!needContent.isEmpty() || !blockedReason.isEmpty() || bReadyWaiting)
    {
        SchedulePump();
    }
}
