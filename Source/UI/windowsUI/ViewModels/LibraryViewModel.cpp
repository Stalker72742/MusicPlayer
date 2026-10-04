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

#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "Playlists/PlaylistSubsystem.h"

namespace
{
    constexpr int MaxRecent = 50;

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

void LibraryViewModel::Rebuild()
{
    const QList<Track>& library = LibrarySubsystem::Get().GetTracks();

    QList<const Track*> sorted;
    sorted.reserve(library.size());
    for (const Track& track : library)
    {
        if (track.IsInLibrary())
            sorted.append(&track);
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(sorted.begin(), sorted.end(), [&collator](const Track* a, const Track* b) {
        if (a->IsOnline() != b->IsOnline())
            return b->IsOnline();
        if (a->IsOnline())
            return a->stats.dateAdded < b->stats.dateAdded;
        return collator.compare(a->file.path, b->file.path) < 0;
    });

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
