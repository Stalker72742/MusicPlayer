//
// Created by Stalker7274 on 03.10.2026.
//

#include "Library/LibrarySubsystem.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <QTomlUtils/QTomlUtils.h>

#include <algorithm>
#include <utility>

#include "AppConfigs.h"
#include "FileScanner.h"
#include "FolderWatcher.h"
#include "LibraryStorage.h"
#include "MetadataReader.h"

namespace
{
    constexpr int NotifyDelayMs = 300;
    constexpr int SaveDelayMs = 2000;
    constexpr int PathsSettleMs = 150;
    constexpr int PathsMaxDelayMs = 1000;
    constexpr qsizetype MaxPathsForPartialScan = 256;

    QString NormalizedPath(const QString& path)
    {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    QStringList CleanedLabels(const QStringList& labels)
    {
        QStringList cleaned;
        for (const QString& label : labels)
        {
            const QString simplified = label.simplified();
            if (!simplified.isEmpty() && !cleaned.contains(simplified, Qt::CaseInsensitive))
                cleaned.append(simplified);
        }
        return cleaned;
    }

    bool IsUnder(const QString& path, const QStringList& folders)
    {
        return std::any_of(folders.cbegin(), folders.cend(), [&path](const QString& folder) {
            if (!path.startsWith(folder, Qt::CaseInsensitive))
                return false;
            return path.size() == folder.size() || folder.endsWith(QLatin1Char('/')) || path[folder.size()] == QLatin1Char('/');
        });
    }

    QString Fingerprint(const QString& path, qint64 size, qint64 modifiedMs)
    {
        const QString name = path.sliced(path.lastIndexOf(QLatin1Char('/')) + 1).toLower();
        return QString::number(size) + QLatin1Char('|') + QString::number(modifiedMs) + QLatin1Char('|') + name;
    }
}

struct LibrarySubsystem::ScanJob
{
    bool bFull = false;
    QStringList scope;
    QList<FileScanner::ScannedFile> files;

    bool IsInScope(const Track& track) const
    {
        if (bFull && !track.IsOnline())
            return true;
        return IsUnder(track.file.path, scope);
    }
};

LibrarySubsystem::LibrarySubsystem()
    : metadataReader(new MetadataReader(this))
    , folderWatcher(new FolderWatcher(this))
{
    notifyTimer.setSingleShot(true);
    notifyTimer.setInterval(NotifyDelayMs);
    connect(&notifyTimer, &QTimer::timeout, this, &LibrarySubsystem::tracksChanged);

    saveTimer.setSingleShot(true);
    saveTimer.setInterval(SaveDelayMs);
    connect(&saveTimer, &QTimer::timeout, this, &LibrarySubsystem::SaveRegistry);

    pathsTimer.setSingleShot(true);
    connect(&pathsTimer, &QTimer::timeout, this, &LibrarySubsystem::StartNextScan);

    connect(folderWatcher, &FolderWatcher::pathsChanged, this, &LibrarySubsystem::OnPathsChanged);
    connect(folderWatcher, &FolderWatcher::rescanNeeded, this, &LibrarySubsystem::Rescan);

    connect(&scanWatcher, &QFutureWatcher<void>::finished, this, &LibrarySubsystem::OnScanFinished);
    connect(metadataReader, &MetadataReader::tagsRead, this, &LibrarySubsystem::OnTagsRead);
    connect(metadataReader, &MetadataReader::idle, this, &LibrarySubsystem::scanStateChanged);
}

void LibrarySubsystem::Deinitialize()
{
    notifyTimer.stop();
    pathsTimer.stop();
    metadataReader->Clear();

    delete folderWatcher;
    folderWatcher = nullptr;

    bStopScan = true;
    scanWatcher.waitForFinished();

    if (saveTimer.isActive())
    {
        saveTimer.stop();
        SaveRegistry();
    }

    delete metadataReader;
    metadataReader = nullptr;
}

void LibrarySubsystem::Initialize()
{
    if (bInitialized)
        return;

    bInitialized = true;
    LoadRegistry();

    folderWatcher->SetRoots(FileScanner::CollapseRoots(GetFolders()));

    Rescan();
}

QStringList LibrarySubsystem::GetFolders() const
{
    return QTomlUtils::FindPropertyValue<QStringList>(AppConfigs::Settings, AppConfigs::SettingsKeys::MusicScanFolders)
        .value_or(QStringList{});
}

void LibrarySubsystem::SetFolders(const QStringList& folders)
{
    QStringList cleaned;
    for (const QString& folder : folders)
    {
        const QString path = NormalizedPath(folder);
        if (!path.isEmpty() && !cleaned.contains(path, Qt::CaseInsensitive))
            cleaned.append(path);
    }

    if (cleaned == GetFolders())
        return;

    QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::MusicScanFolders, cleaned);
    ApplyFoldersChanged();
}

void LibrarySubsystem::AddFolder(const QString& folder)
{
    SetFolders(GetFolders() << folder);
}

void LibrarySubsystem::RemoveFolder(const QString& folder)
{
    QStringList folders = GetFolders();
    folders.removeAll(folder);
    SetFolders(folders);
}

void LibrarySubsystem::ResetFolders()
{
    QTomlUtils::ResetToDefault(AppConfigs::Settings, AppConfigs::SettingsKeys::MusicScanFolders);
    ApplyFoldersChanged();
}

void LibrarySubsystem::ApplyFoldersChanged()
{
    folderWatcher->SetRoots(FileScanner::CollapseRoots(GetFolders()));
    emit foldersChanged();
    Rescan();
}

bool LibrarySubsystem::AreFoldersModified() const
{
    return QTomlUtils::IsOverridden(AppConfigs::Settings, AppConfigs::SettingsKeys::MusicScanFolders);
}

void LibrarySubsystem::Rescan()
{
    bFullScanPending = true;
    StartNextScan();
}

void LibrarySubsystem::OnPathsChanged(const QStringList& paths)
{
    if (pendingPaths.isEmpty())
        pendingSince.start();

    pendingPaths.unite(QSet<QString>(paths.cbegin(), paths.cend()));

    pathsTimer.start(static_cast<int>(std::clamp<qint64>(PathsMaxDelayMs - pendingSince.elapsed(), 0, PathsSettleMs)));
}

void LibrarySubsystem::StartNextScan()
{
    if (scanWatcher.isRunning() || bStopScan)
        return;

    auto job = std::make_shared<ScanJob>();
    job->bFull = bFullScanPending || pendingPaths.size() > MaxPathsForPartialScan;

    if (job->bFull)
    {
        job->scope = FileScanner::CollapseRoots(GetFolders());
        pathsTimer.stop();
    }
    else if (!pendingPaths.isEmpty() && !pathsTimer.isActive())
    {
        job->scope = FileScanner::CollapseRoots(pendingPaths.values());
    }
    else
    {
        return;
    }

    bFullScanPending = false;
    pendingPaths.clear();

    currentJob = job;
    bFullScanRunning = job->bFull;

    scanWatcher.setFuture(QtConcurrent::run([job, stop = &bStopScan] {
        job->files = job->bFull ? FileScanner::ScanTrees(job->scope, *stop) : FileScanner::ScanPaths(job->scope, *stop);
    }));

    if (job->bFull)
        emit scanStateChanged();
}

void LibrarySubsystem::OnScanFinished()
{
    const std::shared_ptr<ScanJob> job = std::move(currentJob);
    if (!job || bStopScan)
        return;

    ApplyScan(*job);

    bFullScanRunning = false;
    if (job->bFull)
        emit scanStateChanged();

    StartNextScan();
}

void LibrarySubsystem::ApplyScan(const ScanJob& job)
{
    QSet<TrackId> found;
    QList<const FileScanner::ScannedFile*> unknown;
    int added = 0;
    int updated = 0;
    int moved = 0;
    bool bStructureChanged = false;
    bool bSaveNeeded = false;

    found.reserve(job.files.size());

    for (const FileScanner::ScannedFile& file : job.files)
    {
        Track* track = FindMutable(idByPath.value(file.path));
        if (!track)
        {
            unknown.append(&file);
            continue;
        }

        found.insert(track->id);

        if (track->file.bMissing)
        {
            track->file.bMissing = false;
            bStructureChanged = true;
        }

        const bool bContentChanged = track->file.size != file.size || track->file.modifiedMs != file.modifiedMs;
        if (bContentChanged || track->file.fileId != file.fileId || track->file.volumeId != file.volumeId)
        {
            track->file.size = file.size;
            track->file.modifiedMs = file.modifiedMs;
            track->file.fileId = file.fileId;
            track->file.volumeId = file.volumeId;
            bSaveNeeded = true;
        }

        if (track->IsOnline())
            continue;

        if (bContentChanged)
        {
            track->tags.bFromFile = false;
            ++updated;
        }

        if (!track->tags.bFromFile)
            metadataReader->Enqueue(track->id, file.path);
    }

    QList<TrackId> gone;
    QHash<std::pair<quint64, quint32>, TrackId> movedById;
    QHash<QString, TrackId> movedByFingerprint;

    for (const Track& track : tracks)
    {
        if (track.file.path.isEmpty() || found.contains(track.id))
            continue;

        const bool bGone = !track.file.bMissing && job.IsInScope(track);
        if (bGone)
            gone.append(track.id);

        if (bGone || track.file.bMissing)
        {
            if (track.file.fileId != 0)
                movedById.insert({track.file.fileId, track.file.volumeId}, track.id);
            movedByFingerprint.insert(Fingerprint(track.file.path, track.file.size, track.file.modifiedMs), track.id);
        }
    }

    for (const FileScanner::ScannedFile* file : unknown)
    {
        TrackId movedId;
        if (file->fileId != 0)
            movedId = movedById.take({file->fileId, file->volumeId});
        if (!movedId.IsValid())
            movedId = movedByFingerprint.take(Fingerprint(file->path, file->size, file->modifiedMs));

        Track* track = FindMutable(movedId);
        if (!track || found.contains(movedId))
        {
            const TrackId id = AddTrack(file->path, file->size, file->modifiedMs, file->fileId, file->volumeId);
            found.insert(id);
            metadataReader->Enqueue(id, file->path);
            ++added;
            continue;
        }

        const bool bContentChanged = track->file.size != file->size || track->file.modifiedMs != file->modifiedMs;

        idByPath.remove(track->file.path);
        idByPath.insert(file->path, track->id);
        track->file = {file->path, file->size, file->modifiedMs, file->fileId, file->volumeId, false};
        found.insert(track->id);
        ++moved;

        if (track->IsOnline())
            continue;

        if (bContentChanged)
            track->tags.bFromFile = false;
        if (!track->tags.bFromFile)
            metadataReader->Enqueue(track->id, file->path);
    }

    int missing = 0;
    for (TrackId id : gone)
    {
        if (found.contains(id))
            continue;

        FindMutable(id)->file.bMissing = true;
        ++missing;
    }

    qDebug() << "[Library]" << (job.bFull ? "Scan:" : "Update:") << job.files.size() << "files," << added << "new," << updated
             << "changed," << moved << "moved," << missing << "missing";

    bStructureChanged |= added || moved || missing;
    if (bStructureChanged)
        ++structureRevision;

    if (bStructureChanged)
    {
        notifyTimer.stop();
        emit tracksChanged();
    }
    else if (updated)
    {
        NotifyChanged();
    }

    if (bStructureChanged || updated || bSaveNeeded)
        ScheduleSave();
}

void LibrarySubsystem::OnTagsRead(TrackId id, const TagsComponent& tags, bool bSuccess)
{
    Track* track = FindMutable(id);
    if (!track)
        return;

    const TagsComponent guessed = GuessTagsFromPath(track->file.path);
    TagsComponent merged = tags;
    if (merged.title.isEmpty())
        merged.title = guessed.title;
    if (merged.artist.isEmpty())
        merged.artist = guessed.artist;
    if (merged.album.isEmpty())
        merged.album = guessed.album;
    if (merged.durationMs <= 0)
        merged.durationMs = track->tags.durationMs;

    merged.bFromFile = true;
    if (!bSuccess)
        qDebug() << "[Library] Cannot read tags of" << track->file.path;

    track->tags = merged;
    ApplyTagEdits(*track);
    NotifyChanged();
    ScheduleSave();
}

const Track* LibrarySubsystem::FindTrack(TrackId id) const
{
    const auto it = indexById.constFind(id);
    return it != indexById.cend() ? &tracks[*it] : nullptr;
}

Track* LibrarySubsystem::FindMutable(TrackId id)
{
    const auto it = indexById.constFind(id);
    return it != indexById.cend() ? &tracks[*it] : nullptr;
}

TrackId LibrarySubsystem::AddTrack(const QString& path, qint64 size, qint64 modifiedMs, quint64 fileId, quint32 volumeId)
{
    Track track;
    track.id = TrackId{nextId++};
    track.file = {path, size, modifiedMs, fileId, volumeId, false};
    track.tags = GuessTagsFromPath(path);
    track.stats.dateAdded = QDateTime::currentDateTime();

    indexById.insert(track.id, tracks.size());
    idByPath.insert(path, track.id);
    tracks.append(track);
    return track.id;
}

TrackId LibrarySubsystem::AddOnlineTrack(const QString& pageUrl, const TagsComponent& tags, const QString& thumbnailUrl,
    const QString& channelUrl)
{
    if (pageUrl.isEmpty())
        return {};

    if (Track* existing = FindMutable(idByUrl.value(pageUrl)))
    {
        if (!existing->stream.bSaved)
        {
            existing->tags = tags;
            ApplyTagEdits(*existing);
            existing->stream.thumbnailUrl = thumbnailUrl;
            if (!channelUrl.isEmpty())
                existing->stream.channelUrl = channelUrl;
            NotifyChanged();
        }
        else if (existing->stream.channelUrl.isEmpty() && !channelUrl.isEmpty())
        {
            existing->stream.channelUrl = channelUrl;
            ScheduleSave();
        }
        return existing->id;
    }

    Track track;
    track.id = TrackId{nextId++};
    track.tags = tags;
    track.tags.bFromFile = true;
    track.stream = {pageUrl, thumbnailUrl, false, channelUrl};

    indexById.insert(track.id, tracks.size());
    idByUrl.insert(pageUrl, track.id);
    tracks.append(track);
    NotifyChanged();
    return track.id;
}

TrackId LibrarySubsystem::FindOnlineTrack(const QString& pageUrl) const
{
    return idByUrl.value(pageUrl);
}

TrackId LibrarySubsystem::FindTrackByPath(const QString& path) const
{
    return path.isEmpty() ? TrackId{} : idByPath.value(NormalizedPath(path));
}

void LibrarySubsystem::SetSaved(TrackId id, bool bSaved)
{
    Track* track = FindMutable(id);
    if (!track || !track->IsOnline() || track->stream.bSaved == bSaved)
        return;

    track->stream.bSaved = bSaved;
    if (bSaved)
        track->stats.dateAdded = QDateTime::currentDateTime();
    else
        track->stats.bLiked = false;

    ++structureRevision;
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::SetLocalFile(TrackId id, const QString& path)
{
    Track* track = FindMutable(id);
    if (!track || !track->IsOnline())
        return;

    const QString normalized = path.isEmpty() ? QString() : NormalizedPath(path);
    if (track->file.path == normalized && !track->file.bMissing)
        return;

    if (!normalized.isEmpty())
    {
        const TrackId owner = idByPath.value(normalized);
        if (owner.IsValid() && owner != id)
            RemoveTrack(owner);
    }

    if (!track->file.path.isEmpty())
        idByPath.remove(track->file.path);

    const QFileInfo info(normalized);
    track->file = normalized.isEmpty() ? FileComponent{}
                                       : FileComponent{normalized, info.size(), info.lastModified().toMSecsSinceEpoch()};
    if (!normalized.isEmpty())
    {
        idByPath.insert(normalized, id);
        if (!track->stream.bSaved)
        {
            track->stream.bSaved = true;
            track->stats.dateAdded = QDateTime::currentDateTime();
        }
    }

    ++structureRevision;
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::SetFilePath(TrackId id, const QString& path)
{
    Track* track = FindMutable(id);
    if (!track || track->file.path.isEmpty() || path.isEmpty())
        return;

    const QString normalized = NormalizedPath(path);
    if (track->file.path == normalized)
        return;

    const TrackId owner = idByPath.value(normalized);
    if (owner.IsValid() && owner != id)
        RemoveTrack(owner);

    idByPath.remove(track->file.path);
    idByPath.insert(normalized, id);
    track->file.path = normalized;
    track->file.bMissing = false;

    if (!track->IsOnline())
    {
        track->tags.bFromFile = false;
        metadataReader->Enqueue(id, normalized);
    }

    ++structureRevision;
    notifyTimer.stop();
    emit tracksChanged();
    ScheduleSave();
}

void LibrarySubsystem::MarkFileGone(TrackId id)
{
    Track* track = FindMutable(id);
    if (!track || track->IsOnline() || track->file.path.isEmpty() || track->file.bMissing)
        return;

    track->file.bMissing = true;
    ++structureRevision;
    notifyTimer.stop();
    emit tracksChanged();
    ScheduleSave();
}

void LibrarySubsystem::ApplyTagEdits(Track& track)
{
    const QMap<QString, QString>& edits = track.user.tagEdits;
    for (auto it = edits.cbegin(); it != edits.cend(); ++it)
    {
        if (it.key() == QLatin1String("title"))
            track.tags.title = it.value();
        else if (it.key() == QLatin1String("artist"))
            track.tags.artist = it.value();
        else if (it.key() == QLatin1String("album"))
            track.tags.album = it.value();
        else if (it.key() == QLatin1String("genre"))
            track.tags.genre = it.value();
        else if (it.key() == QLatin1String("year"))
            track.tags.year = it.value().toInt();
        else if (it.key() == QLatin1String("track"))
            track.tags.trackNumber = it.value().toInt();
    }
}

void LibrarySubsystem::SetTagEdits(TrackId id, const QMap<QString, QString>& edits)
{
    Track* track = FindMutable(id);
    if (!track || edits.isEmpty())
        return;

    for (auto it = edits.cbegin(); it != edits.cend(); ++it)
    {
        const QString value = it.value().trimmed();
        if (it.key() != QLatin1String("title") || !value.isEmpty())
            track->user.tagEdits.insert(it.key(), value);
    }

    ApplyTagEdits(*track);
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::TakeTagsFromFileName(TrackId id)
{
    const Track* track = FindTrack(id);
    if (!track || track->file.path.isEmpty())
        return;

    const TagsComponent guessed = GuessTagsFromPath(track->file.path);
    QMap<QString, QString> edits{{QStringLiteral("title"), guessed.title}};
    if (!guessed.artist.isEmpty())
        edits.insert(QStringLiteral("artist"), guessed.artist);
    SetTagEdits(id, edits);
}

void LibrarySubsystem::SetLabels(TrackId id, const QStringList& labels)
{
    Track* track = FindMutable(id);
    if (!track)
        return;

    const QStringList cleaned = CleanedLabels(labels);
    if (track->user.labels == cleaned)
        return;

    track->user.labels = cleaned;
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::ReplaceUserData(TrackId id, const QStringList& labels, const QMap<QString, QString>& tagEdits)
{
    Track* track = FindMutable(id);
    if (!track)
        return;

    const QStringList cleaned = CleanedLabels(labels);
    QMap<QString, QString> edits;
    for (auto it = tagEdits.cbegin(); it != tagEdits.cend(); ++it)
    {
        const QString value = it.value().trimmed();
        if (it.key() != QLatin1String("title") || !value.isEmpty())
            edits.insert(it.key(), value);
    }

    if (track->user.labels == cleaned && track->user.tagEdits == edits)
        return;

    const bool bDropped = std::any_of(track->user.tagEdits.keyBegin(), track->user.tagEdits.keyEnd(),
        [&edits](const QString& key) { return !edits.contains(key); });

    track->user.labels = cleaned;
    track->user.tagEdits = edits;
    ApplyTagEdits(*track);
    if (bDropped && !track->IsOnline() && !track->file.path.isEmpty())
    {
        track->tags.bFromFile = false;
        metadataReader->Enqueue(id, track->file.path);
    }

    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::RemoveTransientTracks(const QSet<TrackId>& keep)
{
    QList<TrackId> transient;
    for (const Track& track : tracks)
    {
        if (track.IsTransient() && !keep.contains(track.id))
            transient.append(track.id);
    }

    for (TrackId id : transient)
        RemoveTrack(id);

    if (!transient.isEmpty())
        NotifyChanged();
}

void LibrarySubsystem::RemoveTrack(TrackId id)
{
    const auto it = indexById.constFind(id);
    if (it == indexById.cend())
        return;

    const qsizetype index = *it;
    if (tracks[index].IsOnline())
        idByUrl.remove(tracks[index].stream.pageUrl);
    if (!tracks[index].file.path.isEmpty())
        idByPath.remove(tracks[index].file.path);
    indexById.remove(id);
    ++structureRevision;

    const qsizetype last = tracks.size() - 1;
    if (index != last)
    {
        tracks[index] = std::move(tracks[last]);
        indexById[tracks[index].id] = index;
    }
    tracks.removeLast();
}

TagsComponent LibrarySubsystem::GuessTagsFromPath(const QString& path)
{
    const QFileInfo info(path);
    TagsComponent tags;

    const QString baseName = info.completeBaseName();
    const qsizetype separator = baseName.indexOf(QStringLiteral(" - "));
    if (separator > 0)
    {
        tags.artist = baseName.left(separator).trimmed();
        tags.title = baseName.mid(separator + 3).trimmed();
    }
    else
    {
        tags.title = baseName;
    }

    tags.album = info.dir().dirName();
    return tags;
}

void LibrarySubsystem::SetLiked(TrackId id, bool bLiked)
{
    Track* track = FindMutable(id);
    if (!track || track->stats.bLiked == bLiked)
        return;

    track->stats.bLiked = bLiked;
    if (bLiked && track->IsOnline() && !track->stream.bSaved)
    {
        track->stream.bSaved = true;
        track->stats.dateAdded = QDateTime::currentDateTime();
        ++structureRevision;
    }
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::MarkPlayed(TrackId id)
{
    Track* track = FindMutable(id);
    if (!track)
        return;

    track->stats.lastPlayed = QDateTime::currentDateTime();
    ++track->stats.playCount;
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::SetDuration(TrackId id, qint64 durationMs)
{
    Track* track = FindMutable(id);
    if (!track || durationMs <= 0 || track->tags.durationMs == durationMs)
        return;

    track->tags.durationMs = durationMs;
    NotifyChanged();
    ScheduleSave();
}

int LibrarySubsystem::GetPendingMetadataCount() const
{
    return metadataReader ? metadataReader->GetPendingCount() : 0;
}

void LibrarySubsystem::NotifyChanged()
{
    if (!notifyTimer.isActive())
        notifyTimer.start();
}

void LibrarySubsystem::ScheduleSave()
{
    saveTimer.start();
}

QString LibrarySubsystem::GetDataPath(const QString& fileName) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1Char('/') + fileName;
}

void LibrarySubsystem::IndexLoadedTracks(QList<Track> loaded)
{
    tracks.reserve(loaded.size());
    indexById.reserve(loaded.size());
    idByPath.reserve(loaded.size());

    for (Track& track : loaded)
    {
        if (!track.id.IsValid() || indexById.contains(track.id))
            continue;
        if (track.IsOnline() ? idByUrl.contains(track.stream.pageUrl) : track.file.path.isEmpty())
            continue;

        indexById.insert(track.id, tracks.size());
        if (track.IsOnline())
            idByUrl.insert(track.stream.pageUrl, track.id);
        if (!track.file.path.isEmpty())
            idByPath.insert(track.file.path, track.id);
        nextId = std::max(nextId, track.id.value + 1);
        tracks.append(std::move(track));
    }
}

void LibrarySubsystem::LoadRegistry()
{
    const QString path = GetDataPath(QStringLiteral("library.bin"));

    QList<Track> loaded;
    const LibraryStorage::LoadResult result = LibraryStorage::Load(path, loaded);

    if (result == LibraryStorage::LoadResult::Corrupted)
    {
        const QString aside = path + QStringLiteral(".corrupted");
        QFile::remove(aside);
        QFile::rename(path, aside);
        qWarning() << "[Library] The registry is not readable, moved to" << aside;
    }

    if (result != LibraryStorage::LoadResult::Loaded && LibraryStorage::ImportJson(GetDataPath(QStringLiteral("library.json")), loaded))
        ScheduleSave();

    IndexLoadedTracks(std::move(loaded));
    ++structureRevision;

    qDebug() << "[Library] Loaded" << tracks.size() << "tracks";
    emit tracksChanged();
}

void LibrarySubsystem::SaveRegistry()
{
    QList<const Track*> kept;
    kept.reserve(tracks.size());
    for (const Track& track : tracks)
    {
        if (!track.IsTransient())
            kept.append(&track);
    }

    LibraryStorage::Save(GetDataPath(QStringLiteral("library.bin")), kept);
}
