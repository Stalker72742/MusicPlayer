//
// Created by Stalker7274 on 05.11.2026.
//

#include "Online/OnlineLink.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace
{
    const QString WatchUrl = QStringLiteral("https://www.youtube.com/watch?v=");
    const QString PlaylistUrl = QStringLiteral("https://www.youtube.com/playlist?list=");
    const QString SiteUrl = QStringLiteral("https://www.youtube.com/");

    bool IsVideoId(const QString& id)
    {
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]{11}$"));
        return pattern.match(id).hasMatch();
    }

    bool IsPublicPlaylistId(const QString& id)
    {
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]{10,64}$"));
        static const QStringList privatePrefixes{QStringLiteral("RD"), QStringLiteral("UL"), QStringLiteral("TLGG")};

        if (!pattern.match(id).hasMatch())
            return false;
        return std::none_of(privatePrefixes.cbegin(), privatePrefixes.cend(),
            [&id](const QString& prefix) { return id.startsWith(prefix); });
    }

    bool IsChannelId(const QString& id)
    {
        static const QRegularExpression pattern(QStringLiteral("^UC[A-Za-z0-9_-]{22}$"));
        return pattern.match(id).hasMatch();
    }

    bool IsChannelName(const QString& name)
    {
        static const QRegularExpression pattern(QStringLiteral("^[\\p{L}\\p{M}\\p{N}._·-]+$"));
        return pattern.match(name).hasMatch();
    }

    QString ChannelUrl(const QString& path)
    {
        return SiteUrl + QString::fromLatin1(QUrl::toPercentEncoding(path, "@/")) + QStringLiteral("/videos");
    }

    QString FirstEntry(const QString& text)
    {
        for (const QString& line : text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts))
        {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1Char('#')))
                return trimmed;
        }
        return {};
    }

    OnlineLink MakeVideo(const QString& id, const QString& playlistId)
    {
        if (!IsVideoId(id))
            return {};

        OnlineLink link;
        link.kind = OnlineLink::Kind::Video;
        link.url = WatchUrl + id;
        if (IsPublicPlaylistId(playlistId))
            link.playlistUrl = PlaylistUrl + playlistId;
        return link;
    }

    OnlineLink MakePlaylist(const QString& id)
    {
        if (!IsPublicPlaylistId(id))
            return {};

        OnlineLink link;
        link.kind = OnlineLink::Kind::Playlist;
        link.url = PlaylistUrl + id;
        return link;
    }

    OnlineLink MakeChannel(const QString& path)
    {
        OnlineLink link;
        link.kind = OnlineLink::Kind::Channel;
        link.url = ChannelUrl(path);
        return link;
    }
}

OnlineLink OnlineLink::Parse(const QString& text)
{
    QString entry = FirstEntry(text);
    if (entry.isEmpty() || entry.contains(QRegularExpression(QStringLiteral("\\s"))))
        return {};

    if (!entry.contains(QStringLiteral("://")))
        entry.prepend(QStringLiteral("https://"));

    const QUrl url(entry);
    if (!url.isValid() || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")))
        return {};

    static const QSet<QString> siteHosts{
        QStringLiteral("youtube.com"), QStringLiteral("www.youtube.com"), QStringLiteral("m.youtube.com"),
        QStringLiteral("music.youtube.com"), QStringLiteral("youtube-nocookie.com"), QStringLiteral("www.youtube-nocookie.com"),
    };
    static const QSet<QString> shortHosts{QStringLiteral("youtu.be"), QStringLiteral("www.youtu.be")};

    const QString host = url.host().toLower();
    const QStringList path = url.path(QUrl::FullyDecoded).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QUrlQuery query(url);
    const QString list = query.queryItemValue(QStringLiteral("list"));

    if (shortHosts.contains(host))
        return path.isEmpty() ? OnlineLink{} : MakeVideo(path.first(), list);

    if (!siteHosts.contains(host) || path.isEmpty())
        return {};

    const QString& head = path.first();
    const QString second = path.value(1);

    if (head == QStringLiteral("watch"))
    {
        const QString video = query.queryItemValue(QStringLiteral("v"));
        return video.isEmpty() ? MakePlaylist(list) : MakeVideo(video, list);
    }
    if (head == QStringLiteral("playlist"))
        return MakePlaylist(list);

    static const QSet<QString> videoPaths{
        QStringLiteral("shorts"), QStringLiteral("live"), QStringLiteral("embed"), QStringLiteral("v"), QStringLiteral("e"),
    };
    if (videoPaths.contains(head))
        return MakeVideo(second, list);

    if (head.startsWith(QLatin1Char('@')))
        return IsChannelName(head.mid(1)) ? MakeChannel(head) : OnlineLink{};
    if ((head == QStringLiteral("channel") || head == QStringLiteral("browse")) && IsChannelId(second))
        return MakeChannel(QStringLiteral("channel/") + second);
    if ((head == QStringLiteral("c") || head == QStringLiteral("user")) && IsChannelName(second))
        return MakeChannel(head + QLatin1Char('/') + second);

    return {};
}
