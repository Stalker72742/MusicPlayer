//
// Created by Stalker7274 on 04.10.2026.
//

#include "InnerTubeBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace
{
    const QUrl SearchUrl(QStringLiteral("https://www.youtube.com/youtubei/v1/search?prettyPrint=false"));
    const QUrl PlayerUrl(QStringLiteral("https://www.youtube.com/youtubei/v1/player?prettyPrint=false"));

    const QString ClientVersion = QStringLiteral("2.20250925.01.00");
    const QString UserAgent = QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36");

    const QString VideosOnlyParams = QStringLiteral("EgIQAQ%3D%3D");

    constexpr int TimeoutMs = 10'000;

    QString RunsText(const QJsonObject& text)
    {
        if (text.contains("simpleText"))
            return text.value("simpleText").toString();

        QString result;
        for (const QJsonValue& run : text.value("runs").toArray())
            result += run.toObject().value("text").toString();
        return result;
    }
}

void InnerTubeBackend::Shutdown()
{
    CancelSearch();
    if (network)
    {
        for (QNetworkReply* reply : network->findChildren<QNetworkReply*>())
            reply->disconnect(this);
    }
    delete network;
    network = nullptr;
}

void InnerTubeBackend::CancelSearch()
{
    if (!searchReply)
        return;

    searchReply->disconnect(this);
    searchReply->abort();
    searchReply->deleteLater();
    searchReply = nullptr;
}

void InnerTubeBackend::Search(const QString& query, int, SearchCallback callback)
{
    continuation.clear();
    Send({{"query", query}, {"params", VideosOnlyParams}}, std::move(callback));
}

void InnerTubeBackend::SearchMore(SearchCallback callback)
{
    if (continuation.isEmpty())
    {
        OnlineBackend::SearchMore(std::move(callback));
        return;
    }
    Send({{"continuation", continuation}}, std::move(callback));
}

QNetworkReply* InnerTubeBackend::Post(const QUrl& url, QJsonObject body)
{
    if (!network)
        network = new QNetworkAccessManager(this);

    const QLocale locale = QLocale::system();
    const QString region = QLocale::territoryToCode(locale.territory());

    QJsonObject client{
        {"clientName", "WEB"},
        {"clientVersion", ClientVersion},
        {"hl", QLocale::languageToCode(locale.language())},
    };
    if (!region.isEmpty())
        client.insert("gl", region);

    body.insert("context", QJsonObject{{"client", client}});

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, UserAgent);
    request.setRawHeader("X-Youtube-Client-Name", "1");
    request.setRawHeader("X-Youtube-Client-Version", ClientVersion.toUtf8());
    request.setTransferTimeout(TimeoutMs);

    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);

    return network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void InnerTubeBackend::FetchVideo(const QString& pageUrl, VideoCallback callback)
{
    const QString videoId = QUrlQuery(QUrl(pageUrl)).queryItemValue(QStringLiteral("v"));

    QNetworkReply* reply = Post(PlayerUrl, {{"videoId", videoId}});
    connect(reply, &QNetworkReply::finished, this, [reply, pageUrl, videoId, callback] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            callback({}, OnlineError::Make(OnlineError::Failed, QStringLiteral("InnerTube: ") + reply->errorString()));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject details = root.value("videoDetails").toObject();
        if (details.value("videoId").toString() != videoId)
        {
            const QJsonObject playability = root.value("playabilityStatus").toObject();
            const QString reason = playability.value("reason").toString();
            const bool bGone = playability.value("status").toString() == QStringLiteral("ERROR");
            callback({}, OnlineError::Make(bGone ? OnlineError::NotFound : OnlineError::Failed,
                             reason.isEmpty() ? QStringLiteral("InnerTube: no video details") : reason));
            return;
        }

        OnlineEntry entry;
        entry.pageUrl = pageUrl;
        entry.title = details.value("title").toString();
        entry.channel = details.value("author").toString();
        const QString channelId = details.value("channelId").toString();
        if (!channelId.isEmpty())
            entry.channelUrl = QStringLiteral("https://www.youtube.com/channel/") + channelId;
        entry.thumbnailUrl = QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(videoId);
        entry.durationMs = details.value("lengthSeconds").toString().toLongLong() * 1000;
        callback(entry, {});
    });
}

void InnerTubeBackend::Send(QJsonObject body, SearchCallback callback)
{
    CancelSearch();

    QNetworkReply* reply = Post(SearchUrl, std::move(body));
    searchReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback] {
        reply->deleteLater();
        if (searchReply == reply)
            searchReply = nullptr;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError)
        {
            const OnlineError::Kind kind = (status == 403 || status == 429) ? OnlineError::Blocked : OnlineError::Failed;
            callback({}, OnlineError::Make(kind, QStringLiteral("InnerTube: ") + reply->errorString()));
            return;
        }

        QList<OnlineEntry> entries;
        QString next;
        if (!ParsePage(reply->readAll(), entries, next))
        {
            callback({}, OnlineError::Make(OnlineError::Failed, QStringLiteral("InnerTube: unexpected search response")));
            return;
        }

        continuation = next;
        callback(entries, {});
    });
}

bool InnerTubeBackend::ParsePage(const QByteArray& json, QList<OnlineEntry>& entries, QString& nextContinuation)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();

    QJsonArray sections = root.value("contents").toObject()
                              .value("twoColumnSearchResultsRenderer").toObject()
                              .value("primaryContents").toObject()
                              .value("sectionListRenderer").toObject()
                              .value("contents").toArray();
    for (const QJsonValue& command : root.value("onResponseReceivedCommands").toArray())
    {
        const QJsonArray appended =
            command.toObject().value("appendContinuationItemsAction").toObject().value("continuationItems").toArray();
        for (const QJsonValue& section : appended)
            sections.append(section);
    }

    for (const QJsonValue& section : sections)
    {
        const QJsonObject sectionObject = section.toObject();

        const QString token = sectionObject.value("continuationItemRenderer").toObject()
                                  .value("continuationEndpoint").toObject()
                                  .value("continuationCommand").toObject()
                                  .value("token").toString();
        if (!token.isEmpty())
            nextContinuation = token;

        const QJsonArray items = sectionObject.value("itemSectionRenderer").toObject().value("contents").toArray();
        for (const QJsonValue& item : items)
        {
            const QJsonObject video = item.toObject().value("videoRenderer").toObject();
            const QString videoId = video.value("videoId").toString();
            if (videoId.isEmpty())
                continue;

            const qint64 durationMs = ParseDuration(RunsText(video.value("lengthText").toObject()));
            if (durationMs <= 0)
                continue;

            OnlineEntry entry;
            entry.pageUrl = QStringLiteral("https://www.youtube.com/watch?v=") + videoId;
            entry.title = RunsText(video.value("title").toObject());
            const QJsonObject owner = video.value("ownerText").toObject();
            entry.channel = RunsText(owner);
            const QString channelId = owner.value("runs").toArray().first().toObject()
                                          .value("navigationEndpoint").toObject()
                                          .value("browseEndpoint").toObject()
                                          .value("browseId").toString();
            if (!channelId.isEmpty())
                entry.channelUrl = QStringLiteral("https://www.youtube.com/channel/") + channelId;
            entry.thumbnailUrl = QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(videoId);
            entry.durationMs = durationMs;
            entries.append(entry);
        }
    }

    return !sections.isEmpty();
}

qint64 InnerTubeBackend::ParseDuration(const QString& text)
{
    qint64 seconds = 0;
    for (const QString& part : text.split(':', Qt::SkipEmptyParts))
    {
        bool bOk = false;
        const int value = part.toInt(&bOk);
        if (!bOk)
            return 0;
        seconds = seconds * 60 + value;
    }
    return seconds * 1000;
}
