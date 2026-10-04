//
// Created by Stalker7274 on 03.10.2026.
//

#include "Library/LibrarySubsystem.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <QTomlUtils/QTomlUtils.h>

#include "AppConfigs.h"
#include "MetadataReader.h"

namespace
{
    constexpr int CacheVersion = 1;
    constexpr int NotifyDelayMs = 300;
    constexpr int SaveDelayMs = 2000;

    const QStringList AudioFilters{
        QStringLiteral("*.mp3"), QStringLiteral("*.flac"), QStringLiteral("*.wav"), QStringLiteral("*.ogg"),
        QStringLiteral("*.opus"), QStringLiteral("*.m4a"), QStringLiteral("*.aac"), QStringLiteral("*.wma"),
    };

    qint64 ToMs(const QDateTime& time)
    {
        return time.isValid() ? time.toMSecsSinceEpoch() : 0;
    }

    QDateTime FromMs(qint64 ms)
    {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime();
    }

    QString NormalizedPath(const QString& path)
    {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }
}

LibrarySubsystem::LibrarySubsystem()
    : metadataReader(new MetadataReader(this))
{
    notifyTimer.setSingleShot(true);
    notifyTimer.setInterval(NotifyDelayMs);
    connect(&notifyTimer, &QTimer::timeout, this, &LibrarySubsystem::tracksChanged);

    saveTimer.setSingleShot(true);
    saveTimer.setInterval(SaveDelayMs);
    connect(&saveTimer, &QTimer::timeout, this, &LibrarySubsystem::SaveCache);

    connect(&scanWatcher, &QFutureWatcher<QList<ScannedFile>>::finished, this, &LibrarySubsystem::OnScanFinished);
    connect(metadataReader, &MetadataReader::tagsRead, this, &LibrarySubsystem::OnTagsRead);
    connect(metadataReader, &MetadataReader::idle, this, &LibrarySubsystem::scanStateChanged);
}

void LibrarySubsystem::Deinitialize()
{
    notifyTimer.stop();
    metadataReader->Clear();

    scanWatcher.waitForFinished();

    if (saveTimer.isActive())
    {
        saveTimer.stop();
        SaveCache();
    }

    delete metadataReader;
    metadataReader = nullptr;
}

void LibrarySubsystem::Initialize()
{
    if (bInitialized)
        return;

    bInitialized = true;
    LoadCache();

    if (AppConfigs::GetBool(AppConfigs::SettingsKeys::ScanOnStartup, true))
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
    emit foldersChanged();
    Rescan();
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
    emit foldersChanged();
    Rescan();
}

bool LibrarySubsystem::AreFoldersModified() const
{
    return QTomlUtils::IsOverridden(AppConfigs::Settings, AppConfigs::SettingsKeys::MusicScanFolders);
}

void LibrarySubsystem::Rescan()
{
    if (scanWatcher.isRunning())
    {
        bRescanPending = true;
        return;
    }

    scanWatcher.setFuture(QtConcurrent::run(&LibrarySubsystem::ScanFolders, GetFolders()));
    emit scanStateChanged();
}

QList<LibrarySubsystem::ScannedFile> LibrarySubsystem::ScanFolders(const QStringList& folders)
{
    QList<ScannedFile> files;
    QSet<QString> seen;

    for (const QString& folder : folders)
    {
        QDirIterator it(folder, AudioFilters, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QFileInfo info = it.nextFileInfo();
            const QString path = NormalizedPath(info.filePath());

            if (seen.contains(path))
                continue;
            seen.insert(path);

            files.append({path, info.size(), info.lastModified()});
        }
    }
    return files;
}

void LibrarySubsystem::OnScanFinished()
{
    const QList<ScannedFile> files = scanWatcher.result();

    QSet<TrackId> found;
    int added = 0;
    int updated = 0;

    for (const ScannedFile& file : files)
    {
        const TrackId existingId = idByPath.value(file.path);
        Track* track = FindMutable(existingId);

        if (!track)
        {
            const TrackId id = AddTrack(file);
            found.insert(id);
            metadataReader->Enqueue(id, file.path);
            ++added;
            continue;
        }

        found.insert(track->id);

        if (track->IsOnline())
        {
            track->file.size = file.size;
            track->file.modified = file.modified;
            continue;
        }

        if (track->file.size != file.size || track->file.modified != file.modified)
        {
            track->file.size = file.size;
            track->file.modified = file.modified;
            track->tags.bFromFile = false;
            ++updated;
        }

        if (!track->tags.bFromFile)
            metadataReader->Enqueue(track->id, file.path);
    }

    QList<TrackId> missing;
    for (const Track& track : tracks)
    {
        if (!track.IsOnline() && !found.contains(track.id))
            missing.append(track.id);
    }
    for (TrackId id : missing)
        RemoveTrack(id);

    qDebug() << "[Library] Scan finished:" << tracks.size() << "tracks," << added << "new," << updated << "changed,"
             << missing.size() << "removed";

    if (added || updated || !missing.isEmpty())
    {
        NotifyChanged();
        ScheduleSave();
    }

    emit scanStateChanged();

    if (bRescanPending)
    {
        bRescanPending = false;
        Rescan();
    }
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

TrackId LibrarySubsystem::AddTrack(const ScannedFile& file)
{
    Track track;
    track.id = TrackId{nextId++};
    track.file = {file.path, file.size, file.modified};
    track.tags = GuessTagsFromPath(file.path);
    track.stats.dateAdded = QDateTime::currentDateTime();

    indexById.insert(track.id, tracks.size());
    idByPath.insert(file.path, track.id);
    tracks.append(track);
    return track.id;
}

TrackId LibrarySubsystem::AddOnlineTrack(const QString& pageUrl, const TagsComponent& tags, const QString& thumbnailUrl)
{
    if (pageUrl.isEmpty())
        return {};

    if (Track* existing = FindMutable(idByUrl.value(pageUrl)))
    {
        if (!existing->stream.bSaved)
        {
            existing->tags = tags;
            existing->stream.thumbnailUrl = thumbnailUrl;
            NotifyChanged();
        }
        return existing->id;
    }

    Track track;
    track.id = TrackId{nextId++};
    track.tags = tags;
    track.tags.bFromFile = true;
    track.stream = {pageUrl, thumbnailUrl, false};

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

    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::SetLocalFile(TrackId id, const QString& path)
{
    Track* track = FindMutable(id);
    if (!track || !track->IsOnline())
        return;

    const QString normalized = path.isEmpty() ? QString() : NormalizedPath(path);
    if (track->file.path == normalized)
        return;

    if (!normalized.isEmpty())
    {
        const TrackId owner = idByPath.value(normalized);
        if (owner.IsValid() && owner != id)
            RemoveTrack(owner);
    }

    if (track->HasLocalFile())
        idByPath.remove(track->file.path);

    const QFileInfo info(normalized);
    track->file = normalized.isEmpty() ? FileComponent{} : FileComponent{normalized, info.size(), info.lastModified()};
    if (!normalized.isEmpty())
    {
        idByPath.insert(normalized, id);
        if (!track->stream.bSaved)
        {
            track->stream.bSaved = true;
            track->stats.dateAdded = QDateTime::currentDateTime();
        }
    }

    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::SetLabels(TrackId id, const QStringList& labels)
{
    Track* track = FindMutable(id);
    if (!track)
        return;

    QStringList cleaned;
    for (const QString& label : labels)
    {
        const QString simplified = label.simplified();
        if (!simplified.isEmpty() && !cleaned.contains(simplified, Qt::CaseInsensitive))
            cleaned.append(simplified);
    }

    if (track->user.labels == cleaned)
        return;

    track->user.labels = cleaned;
    NotifyChanged();
    ScheduleSave();
}

void LibrarySubsystem::RemoveTransientTracks(const QSet<TrackId>& keep)
{
    QList<TrackId> transient;
    for (const Track& track : tracks)
    {
        if (!track.IsInLibrary() && !keep.contains(track.id))
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
    if (tracks[index].HasLocalFile())
        idByPath.remove(tracks[index].file.path);
    indexById.remove(id);

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

QString LibrarySubsystem::GetCachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/library.json";
}

void LibrarySubsystem::LoadCache()
{
    QFile file(GetCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value("version").toInt() != CacheVersion)
    {
        qWarning() << "[Library] Ignoring a cache of another version:" << file.fileName();
        return;
    }

    for (const QJsonValue& value : root.value("tracks").toArray())
    {
        const QJsonObject object = value.toObject();

        Track track;
        track.id = TrackId{static_cast<quint32>(object.value("id").toInteger())};
        track.file.path = object.value("path").toString();
        track.file.size = object.value("size").toInteger();
        track.file.modified = FromMs(object.value("modified").toInteger());

        track.tags.title = object.value("title").toString();
        track.tags.artist = object.value("artist").toString();
        track.tags.album = object.value("album").toString();
        track.tags.genre = object.value("genre").toString();
        track.tags.trackNumber = object.value("trackNumber").toInt();
        track.tags.year = object.value("year").toInt();
        track.tags.durationMs = object.value("durationMs").toInteger();
        track.tags.bFromFile = object.value("tagsFromFile").toBool();

        track.stats.dateAdded = FromMs(object.value("dateAdded").toInteger());
        track.stats.lastPlayed = FromMs(object.value("lastPlayed").toInteger());
        track.stats.playCount = object.value("playCount").toInt();
        track.stats.bLiked = object.value("liked").toBool();

        for (const QJsonValue& label : object.value("labels").toArray())
            track.user.labels.append(label.toString());

        track.stream.pageUrl = object.value("url").toString();
        track.stream.thumbnailUrl = object.value("thumbnail").toString();
        track.stream.bSaved = track.IsOnline();

        if (!track.id.IsValid() || indexById.contains(track.id))
            continue;
        if (track.IsOnline() ? idByUrl.contains(track.stream.pageUrl) : track.file.path.isEmpty())
            continue;

        indexById.insert(track.id, tracks.size());
        if (track.IsOnline())
            idByUrl.insert(track.stream.pageUrl, track.id);
        if (track.HasLocalFile())
            idByPath.insert(track.file.path, track.id);
        nextId = std::max(nextId, track.id.value + 1);
        tracks.append(track);
    }

    qDebug() << "[Library] Loaded" << tracks.size() << "tracks from cache";
    emit tracksChanged();
}

void LibrarySubsystem::SaveCache()
{
    QJsonArray array;
    for (const Track& track : tracks)
    {
        if (!track.IsInLibrary())
            continue;

        QJsonObject object{
            {"id", static_cast<qint64>(track.id.value)},
            {"path", track.file.path},
            {"size", track.file.size},
            {"modified", ToMs(track.file.modified)},
            {"title", track.tags.title},
            {"artist", track.tags.artist},
            {"album", track.tags.album},
            {"genre", track.tags.genre},
            {"trackNumber", track.tags.trackNumber},
            {"year", track.tags.year},
            {"durationMs", track.tags.durationMs},
            {"tagsFromFile", track.tags.bFromFile},
            {"dateAdded", ToMs(track.stats.dateAdded)},
            {"lastPlayed", ToMs(track.stats.lastPlayed)},
            {"playCount", track.stats.playCount},
            {"liked", track.stats.bLiked},
        };

        if (!track.user.labels.isEmpty())
            object.insert("labels", QJsonArray::fromStringList(track.user.labels));

        if (track.IsOnline())
        {
            object.insert("url", track.stream.pageUrl);
            object.insert("thumbnail", track.stream.thumbnailUrl);
        }
        array.append(object);
    }

    const QString path = GetCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "[Library] Cannot save cache:" << file.errorString();
        return;
    }

    file.write(QJsonDocument(QJsonObject{{"version", CacheVersion}, {"tracks", array}}).toJson(QJsonDocument::Compact));
    if (!file.commit())
        qWarning() << "[Library] Cannot save cache:" << file.errorString();
}
