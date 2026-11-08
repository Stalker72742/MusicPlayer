#include "OnlinePageViewModel.h"

#include <QDesktopServices>
#include <QJSEngine>
#include <QUrl>

#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "Navigation.h"
#include "Online/OnlineLink.h"
#include "Online/OnlineSubsystem.h"
#include "Player/PlayerSubsystem.h"
#include "Playlists/PlaylistSubsystem.h"
#include "SearchViewModel.h"

namespace
{
    constexpr int MaxPlaylistTracks = 1000;
    constexpr int MaxChannelTracks = 500;
    constexpr int RebuildDelayMs = 100;

    QList<TrackId> ToTrackIds(const QVariantList& values)
    {
        QList<TrackId> tracks;
        tracks.reserve(values.size());
        for (const QVariant& value : values)
            tracks.append(TrackId{static_cast<quint32>(value.toInt())});
        return tracks;
    }

    QString KindName(OnlineLink::Kind kind)
    {
        switch (kind)
        {
            case OnlineLink::Kind::Video:
                return QStringLiteral("video");
            case OnlineLink::Kind::Playlist:
                return QStringLiteral("playlist");
            case OnlineLink::Kind::Channel:
                return QStringLiteral("channel");
            case OnlineLink::Kind::None:
                break;
        }
        return {};
    }
}

OnlinePageViewModel::OnlinePageViewModel()
{
    rebuildTimer.setSingleShot(true);
    rebuildTimer.setInterval(RebuildDelayMs);
    connect(&rebuildTimer, &QTimer::timeout, this, &OnlinePageViewModel::Rebuild);

    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, [this] {
        if (!ids.isEmpty())
            rebuildTimer.start();
    });

    const auto follow = [this] {
        const QString target = Navigation::Get().GetOnlineUrl();
        if (Navigation::Get().GetPage() == Navigation::OnlinePage && !target.isEmpty() && target != url)
        {
            pendingAction = OpenOnly;
            playlistUrl.clear();
            Load(target);
        }
    };
    connect(&Navigation::Get(), &Navigation::onlineUrlChanged, this, follow);
    connect(&Navigation::Get(), &Navigation::pageChanged, this, follow);
    follow();
}

OnlinePageViewModel& OnlinePageViewModel::Get()
{
    static OnlinePageViewModel instance;
    return instance;
}

OnlinePageViewModel* OnlinePageViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

QString OnlinePageViewModel::linkKind(const QString& text) const
{
    return KindName(OnlineLink::Parse(text).kind);
}

bool OnlinePageViewModel::openLink(const QString& text, LinkAction action)
{
    const OnlineLink link = OnlineLink::Parse(text);
    if (!link.IsValid())
        return false;

    pendingAction = action;

    if (link.url == url && state != Failed)
    {
        if (!link.playlistUrl.isEmpty())
        {
            playlistUrl = link.playlistUrl;
            emit changed();
        }
        if (state == Done)
            ApplyPendingAction();
    }
    else
    {
        playlistUrl = link.playlistUrl;
        Load(link.url);
    }

    Navigation::Get().openOnline(link.url);
    return true;
}

void OnlinePageViewModel::openAuthor(int trackId)
{
    const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(trackId)});
    if (!track || !track->IsOnline())
        return;

    if (openLink(track->stream.channelUrl))
        return;

    const QString artist = track->tags.artist;
    OnlineSubsystem::Get().FetchVideo(track->stream.pageUrl, this, [this, artist](const OnlineEntry& entry, const QString&) {
        if (entry.channelUrl.isEmpty())
        {
            SearchViewModel::Get().searchOnline(artist);
            return;
        }

        TagsComponent tags;
        OnlineSubsystem::GuessArtistAndTitle(entry, tags.artist, tags.title);
        tags.album = entry.channel;
        tags.durationMs = entry.durationMs;
        LibrarySubsystem::Get().AddOnlineTrack(entry.pageUrl, tags, entry.thumbnailUrl, entry.channelUrl);

        openLink(entry.channelUrl);
    });
}

void OnlinePageViewModel::openPlaylist()
{
    openLink(playlistUrl);
}

void OnlinePageViewModel::openChannel()
{
    openLink(channelUrl);
}

void OnlinePageViewModel::openInBrowser()
{
    if (!url.isEmpty())
        QDesktopServices::openUrl(QUrl(url));
}

void OnlinePageViewModel::retry()
{
    if (!url.isEmpty())
        Load(url);
}

void OnlinePageViewModel::Reset()
{
    ++generation;
    if (listing != 0)
        OnlineSubsystem::Get().CancelListing(listing);
    listing = 0;

    error.clear();
    title.clear();
    channel.clear();
    channelUrl.clear();
    bLimitReached = false;
    ids.clear();
    idSet.clear();
}

void OnlinePageViewModel::Load(const QString& pageUrl)
{
    const OnlineLink link = OnlineLink::Parse(pageUrl);
    if (!link.IsValid())
        return;

    Reset();
    DropUnusedTransientTracks();

    url = link.url;
    kind = KindName(link.kind);
    state = Loading;
    Rebuild();
    emit changed();

    const quint64 current = generation;

    if (link.kind == OnlineLink::Kind::Video)
    {
        limit = 1;
        OnlineSubsystem::Get().FetchVideo(url, this, [this, current](const OnlineEntry& entry, const QString& failure) {
            if (current != generation)
                return;

            if (failure.isEmpty())
            {
                title = entry.title;
                channel = entry.channel;
                channelUrl = entry.channelUrl;
                AddEntry(entry);
            }
            Finish(failure);
        });
        return;
    }

    limit = link.kind == OnlineLink::Kind::Channel ? MaxChannelTracks : MaxPlaylistTracks;
    StartListing(url, link.kind == OnlineLink::Kind::Channel);
}

void OnlinePageViewModel::StartListing(const QString& listUrl, bool bChannel)
{
    const quint64 current = generation;

    listing = OnlineSubsystem::Get().StartListing(listUrl, limit, this,
        [this, current, bChannel](const OnlineListItem& item) {
            if (current != generation || item.kind != OnlineListItem::Kind::Video)
                return;

            const QString newTitle = bChannel ? item.listChannel : item.listTitle;
            if (title != newTitle || channel != item.listChannel || channelUrl != item.listChannelUrl)
            {
                title = newTitle;
                channel = item.listChannel;
                channelUrl = item.listChannelUrl;
                emit changed();
            }
            AddEntry(item.entry);
        },
        [this, current, listUrl, bChannel](const QString& failure) {
            if (current != generation)
                return;

            listing = 0;

            const QString videosTab = QStringLiteral("/videos");
            if (bChannel && listUrl.endsWith(videosTab) && failure.contains(QStringLiteral("does not have a videos tab")))
            {
                StartListing(listUrl.chopped(videosTab.size()), bChannel);
                return;
            }

            bLimitReached = ids.size() >= limit;
            Finish(failure);
        });
}

void OnlinePageViewModel::AddEntry(const OnlineEntry& entry)
{
    TagsComponent tags;
    OnlineSubsystem::GuessArtistAndTitle(entry, tags.artist, tags.title);
    tags.album = entry.channel;
    tags.durationMs = entry.durationMs;

    const TrackId id = LibrarySubsystem::Get().AddOnlineTrack(entry.pageUrl, tags, entry.thumbnailUrl, entry.channelUrl);
    if (!id.IsValid() || idSet.contains(id))
        return;

    idSet.insert(id);
    ids.append(id);
    rebuildTimer.start();
}

void OnlinePageViewModel::Finish(const QString& failure)
{
    error = failure;
    state = failure.isEmpty() ? Done : Failed;
    Rebuild();
    emit changed();

    if (state == Done)
        ApplyPendingAction();
    else
        pendingAction = OpenOnly;
}

void OnlinePageViewModel::ApplyPendingAction()
{
    const LinkAction action = pendingAction;
    pendingAction = OpenOnly;

    if (action == AddToLibrary)
        addToLibrary(allTrackIds());
    else if (action == Download)
        download(allTrackIds());
}

QVariantList OnlinePageViewModel::allTrackIds() const
{
    QVariantList values;
    values.reserve(ids.size());
    for (TrackId id : ids)
        values.append(static_cast<int>(id.value));
    return values;
}

int OnlinePageViewModel::download(const QVariantList& trackIds)
{
    return DownloadSubsystem::Get().Enqueue(ToTrackIds(trackIds));
}

int OnlinePageViewModel::addToLibrary(const QVariantList& trackIds)
{
    LibrarySubsystem& library = LibrarySubsystem::Get();

    int added = 0;
    for (TrackId id : ToTrackIds(trackIds))
    {
        const Track* track = library.FindTrack(id);
        if (!track || track->IsInLibrary())
            continue;

        library.SetSaved(id, true);
        ++added;
    }
    return added;
}

void OnlinePageViewModel::saveAsPlaylist(const QString& name, const QVariantList& trackIds)
{
    const QString playlist = PlaylistSubsystem::Get().CreateStatic(name, ToTrackIds(trackIds));
    Navigation::Get().openPlaylist(playlist);
}

void OnlinePageViewModel::Rebuild()
{
    QList<TrackData> data;
    data.reserve(ids.size());
    for (TrackId id : ids)
    {
        if (const Track* track = LibrarySubsystem::Get().FindTrack(id))
            data.append(LibraryViewModel::ToTrackData(*track));
    }
    tracksModel.SetTracks(data);
}

void OnlinePageViewModel::DropUnusedTransientTracks()
{
    QSet<TrackId> keep = SearchViewModel::Get().GetResultTracks();
    keep.unite(Get().idSet);

    const PlayerSubsystem& player = PlayerSubsystem::Get();
    keep.insert(player.GetCurrentTrack());
    for (TrackId id : player.GetQueue())
        keep.insert(id);

    LibrarySubsystem::Get().RemoveTransientTracks(keep);
}
