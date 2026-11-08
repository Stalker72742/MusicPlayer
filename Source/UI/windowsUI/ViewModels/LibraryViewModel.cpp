#include "LibraryViewModel.h"

#include <QClipboard>
#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSEngine>
#include <QProcess>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <vector>

#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "Player/PlayerSubsystem.h"
#include "Playlists/PlaylistSubsystem.h"

namespace
{
    constexpr int MaxRecent = 50;

    TrackId ToId(const QVariant& value)
    {
        return TrackId{static_cast<quint32>(value.toInt())};
    }

    bool HasFile(const Track& track)
    {
        return track.HasLocalFile() && !track.file.path.isEmpty();
    }

    QString CheckFileName(const QString& name)
    {
        static const QString Forbidden = QStringLiteral("\\/:*?\"<>|");
        if (name.isEmpty())
            return QStringLiteral("The name is empty.");
        for (const QChar ch : name)
        {
            if (Forbidden.contains(ch) || ch.unicode() < 32)
                return QStringLiteral("A file name cannot contain \\ / : * ? \" < > |");
        }
        if (name.endsWith('.') || name.endsWith(' '))
            return QStringLiteral("A file name cannot end with a dot or a space.");
        return {};
    }

    QColor AlbumTint(const QString& album)
    {
        const int hue = static_cast<int>(qHash(album) % 360);
        return QColor::fromHsl(hue, 70, 42);
    }
}

LibraryViewModel::LibraryViewModel()
{
    LibrarySubsystem& library = LibrarySubsystem::Get();
    connect(&library, &LibrarySubsystem::tracksChanged, this, &LibraryViewModel::Rebuild);
    connect(&library, &LibrarySubsystem::scanStateChanged, this, &LibraryViewModel::scanStateChanged);

    Rebuild();
}

LibraryViewModel& LibraryViewModel::Get()
{
    static LibraryViewModel instance;
    return instance;
}

LibraryViewModel* LibraryViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

bool LibraryViewModel::IsScanning() const
{
    return LibrarySubsystem::Get().IsScanning();
}

int LibraryViewModel::GetPendingMetadata() const
{
    return LibrarySubsystem::Get().GetPendingMetadataCount();
}

void LibraryViewModel::toggleLiked(int trackId)
{
    const TrackId id{static_cast<quint32>(trackId)};
    if (const Track* track = LibrarySubsystem::Get().FindTrack(id))
        LibrarySubsystem::Get().SetLiked(id, !track->stats.bLiked);
}

void LibraryViewModel::rescan()
{
    LibrarySubsystem::Get().Rescan();
}

void LibraryViewModel::setInLibrary(int trackId, bool bValue)
{
    const TrackId id{static_cast<quint32>(trackId)};
    if (!bValue)
    {
        PlaylistSubsystem::Get().ForgetTrack(id);
        DownloadSubsystem::Get().Cancel(id);
    }
    LibrarySubsystem::Get().SetSaved(id, bValue);
}

QString LibraryViewModel::labelsText(int trackId) const
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    return track ? track->user.labels.join(QStringLiteral(", ")) : QString();
}

void LibraryViewModel::setLabelsText(int trackId, const QString& text)
{
    LibrarySubsystem::Get().SetLabels(TrackId{static_cast<quint32>(trackId)}, text.split(','));
}

void LibraryViewModel::openSource(int trackId)
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    if (!track)
        return;

    if (track->IsOnline())
    {
        QDesktopServices::openUrl(QUrl(track->stream.pageUrl));
        return;
    }

#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(track->file.path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(track->file.path).absolutePath()));
#endif
}

void LibraryViewModel::copyLink(int trackId)
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    if (track)
        QGuiApplication::clipboard()->setText(track->IsOnline() ? track->stream.pageUrl : track->file.path);
}

void LibraryViewModel::showInFolder(int trackId)
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    if (!track || !HasFile(*track))
        return;

#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(track->file.path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(track->file.path).absolutePath()));
#endif
}

void LibraryViewModel::copyFilePath(int trackId)
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    if (track && HasFile(*track))
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(track->file.path));
}

QString LibraryViewModel::fileBaseName(int trackId) const
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    return track && HasFile(*track) ? QFileInfo(track->file.path).completeBaseName() : QString();
}

QString LibraryViewModel::renameFile(int trackId, const QString& baseName)
{
    const TrackId id{static_cast<quint32>(trackId)};
    const Track* track = LibrarySubsystem::Get().FindTrack(id);
    if (!track || !HasFile(*track))
        return QStringLiteral("This track has no file.");

    const QString name = baseName.trimmed();
    if (const QString problem = CheckFileName(name); !problem.isEmpty())
        return problem;

    const QFileInfo info(track->file.path);
    const QString suffix = info.suffix();
    const QString target = info.dir().filePath(suffix.isEmpty() ? name : name + '.' + suffix);
    if (target == info.filePath())
        return {};

    const bool bOnlyCase = target.compare(info.filePath(), Qt::CaseInsensitive) == 0;
    if (!bOnlyCase && QFileInfo::exists(target))
        return QStringLiteral("A file with this name is already there.");

    PlayerSubsystem& player = PlayerSubsystem::Get();
    const bool bReleased = player.ReleaseFile(id);

    QFile file(info.filePath());
    const bool bRenamed = file.rename(target);
    if (bRenamed)
    {
        LibrarySubsystem::Get().SetFilePath(id, target);
        LibrarySubsystem::Get().TakeTagsFromFileName(id);
    }

    if (bReleased)
        player.RestoreFile();

    if (!bRenamed)
        return QStringLiteral("Cannot rename the file: ") + file.errorString();
    return {};
}

void LibraryViewModel::openProperties(const QVariantList& trackIds)
{
    if (!trackIds.isEmpty())
        emit propertiesRequested(trackIds);
}

QVariantMap LibraryViewModel::trackProperties(int trackId) const
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    if (!track)
        return {{QStringLiteral("id"), 0}};

    const bool bHasFile = HasFile(*track);
    const qint64 seconds = track->tags.durationMs / 1000;
    return {
        {QStringLiteral("id"), trackId},
        {QStringLiteral("title"), track->tags.title},
        {QStringLiteral("artist"), track->tags.artist},
        {QStringLiteral("album"), track->tags.album},
        {QStringLiteral("genre"), track->tags.genre},
        {QStringLiteral("year"), track->tags.year > 0 ? QString::number(track->tags.year) : QString()},
        {QStringLiteral("track"), track->tags.trackNumber > 0 ? QString::number(track->tags.trackNumber) : QString()},
        {QStringLiteral("tags"), track->user.labels.join(QStringLiteral(", "))},
        {QStringLiteral("fileName"), bHasFile ? QFileInfo(track->file.path).completeBaseName() : QString()},
        {QStringLiteral("path"), bHasFile ? QDir::toNativeSeparators(track->file.path) : track->stream.pageUrl},
        {QStringLiteral("hasFile"), bHasFile},
        {QStringLiteral("duration"), QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'))},
        {QStringLiteral("artUrl"), track->stream.thumbnailUrl},
        {QStringLiteral("liked"), track->stats.bLiked},
        {QStringLiteral("online"), track->IsOnline()},
        {QStringLiteral("inLibrary"), track->IsInLibrary()},
        {QStringLiteral("downloaded"), track->IsOnline() && track->HasLocalFile()},
    };
}

void LibraryViewModel::setTrackTags(int trackId, const QVariantMap& tags)
{
    QMap<QString, QString> edits;
    for (auto it = tags.cbegin(); it != tags.cend(); ++it)
        edits.insert(it.key(), it.value().toString());
    LibrarySubsystem::Get().SetTagEdits(TrackId{static_cast<quint32>(trackId)}, edits);
}

void LibraryViewModel::copyText(const QString& text)
{
    QGuiApplication::clipboard()->setText(text);
}

QVariantMap LibraryViewModel::describeTracks(const QVariantList& trackIds) const
{
    int files = 0;
    int downloads = 0;
    int saved = 0;
    int downloadable = 0;
    int total = 0;
    bool bAllLiked = true;
    for (const QVariant& value : trackIds)
    {
        const Track* track = LibrarySubsystem::Get().FindTrack(ToId(value));
        if (!track)
            continue;

        ++total;
        bAllLiked = bAllLiked && track->stats.bLiked;
        if (!track->IsOnline())
        {
            ++files;
        }
        else if (HasFile(*track))
        {
            ++downloads;
        }
        else
        {
            ++downloadable;
            if (track->IsInLibrary())
                ++saved;
        }
    }

    return {
        {QStringLiteral("files"), files},
        {QStringLiteral("downloads"), downloads},
        {QStringLiteral("saved"), saved},
        {QStringLiteral("downloadable"), downloadable},
        {QStringLiteral("liked"), total > 0 && bAllLiked},
        {QStringLiteral("total"), total},
    };
}

QVariantMap LibraryViewModel::removeTracks(const QVariantList& trackIds)
{
    LibrarySubsystem& library = LibrarySubsystem::Get();
    PlayerSubsystem& player = PlayerSubsystem::Get();

    int removed = 0;
    int failed = 0;
    QString error;

    for (const QVariant& value : trackIds)
    {
        const TrackId id = ToId(value);
        const Track* track = library.FindTrack(id);
        if (!track)
            continue;

        if (track->IsOnline())
        {
            if (HasFile(*track))
                DownloadSubsystem::Get().RemoveDownload(id);
            else if (track->IsInLibrary())
                setInLibrary(static_cast<int>(id.value), false);
            else
                continue;
            ++removed;
            continue;
        }

        const QString path = track->file.path;
        const bool bReleased = player.ReleaseFile(id);

        QFile file(path);
        const bool bTrashed = !file.exists() || file.moveToTrash();
        if (bTrashed)
            library.MarkFileGone(id);

        if (bReleased)
            player.RestoreFile();

        if (bTrashed)
        {
            ++removed;
            continue;
        }

        ++failed;
        if (error.isEmpty())
            error = QFileInfo(path).fileName() + QStringLiteral(": ") + file.errorString();
        qWarning() << "[Library] Cannot move to the Recycle Bin:" << path << file.errorString();
    }

    return {
        {QStringLiteral("removed"), removed},
        {QStringLiteral("failed"), failed},
        {QStringLiteral("error"), error},
    };
}

void LibraryViewModel::toggleLikedTracks(const QVariantList& trackIds)
{
    LibrarySubsystem& library = LibrarySubsystem::Get();

    bool bAllLiked = true;
    for (const QVariant& value : trackIds)
    {
        if (const Track* track = library.FindTrack(ToId(value)); track && !track->stats.bLiked)
            bAllLiked = false;
    }

    for (const QVariant& value : trackIds)
        library.SetLiked(ToId(value), !bAllLiked);
}

TrackData LibraryViewModel::ToTrackData(const Track& track)
{
    TrackData data;
    data.id = static_cast<int>(track.id.value);
    data.title = track.tags.title;
    data.artist = track.tags.artist;
    data.album = track.tags.album;
    data.dateAdded = track.stats.dateAdded.date();
    data.durationSec = static_cast<int>(track.tags.durationMs / 1000);
    data.bLiked = track.stats.bLiked;
    data.artTint = AlbumTint(track.tags.album);
    data.artUrl = track.stream.thumbnailUrl;
    data.bOnline = track.IsOnline();
    data.bInLibrary = track.IsInLibrary();
    data.bDownloaded = track.IsOnline() && track.HasLocalFile();
    return data;
}

void LibraryViewModel::SortTracks()
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    struct Row
    {
        const Track* track;
        QCollatorSortKey key;
    };

    std::vector<Row> local;
    std::vector<const Track*> online;
    for (const Track& track : LibrarySubsystem::Get().GetTracks())
    {
        if (!track.IsInLibrary())
            continue;

        if (track.IsOnline())
            online.push_back(&track);
        else
            local.push_back({&track, collator.sortKey(track.file.path)});
    }

    std::sort(local.begin(), local.end(), [](const Row& a, const Row& b) { return a.key < b.key; });
    std::sort(online.begin(), online.end(),
        [](const Track* a, const Track* b) { return a->stats.dateAdded < b->stats.dateAdded; });

    sortedIds.clear();
    sortedIds.reserve(static_cast<qsizetype>(local.size() + online.size()));
    for (const Row& row : local)
        sortedIds.append(row.track->id);
    for (const Track* track : online)
        sortedIds.append(track->id);
}

void LibraryViewModel::Rebuild()
{
    const LibrarySubsystem& subsystem = LibrarySubsystem::Get();

    if (sortedRevision != subsystem.GetStructureRevision())
    {
        sortedRevision = subsystem.GetStructureRevision();
        SortTracks();
    }

    QList<const Track*> sorted;
    sorted.reserve(sortedIds.size());
    for (TrackId id : sortedIds)
    {
        if (const Track* track = subsystem.FindTrack(id))
            sorted.append(track);
    }

    QList<TrackData> all;
    QList<TrackData> favorites;
    QList<TrackData> online;
    QList<const Track*> played;
    QSet<QString> albums;

    for (const Track* track : sorted)
    {
        const TrackData data = ToTrackData(*track);
        all.append(data);

        if (track->stats.bLiked)
            favorites.append(data);
        if (track->stats.lastPlayed.isValid())
            played.append(track);
        if (track->IsOnline())
            online.prepend(data);
        else if (!track->tags.album.isEmpty())
            albums.insert(track->tags.album);
    }

    std::sort(played.begin(), played.end(),
        [](const Track* a, const Track* b) { return a->stats.lastPlayed > b->stats.lastPlayed; });

    QList<TrackData> recent;
    for (const Track* track : played.first(std::min<qsizetype>(played.size(), MaxRecent)))
        recent.append(ToTrackData(*track));

    tracksModel.SetTracks(all);
    favoritesModel.SetTracks(favorites);
    recentModel.SetTracks(recent);
    onlineModel.SetTracks(online);

    emit scanStateChanged();

    if (albumCount != albums.size())
    {
        albumCount = static_cast<int>(albums.size());
        emit libraryChanged();
    }
}
