//
// Created by Stalker7274 on 04.10.2026.
//

#include "YtDlpBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <memory>

#include "AppConfigs.h"
#include "YtDlp/DenoSubsystem.h"
#include "YtDlp/YtDlpSubsystem.h"

namespace
{
    constexpr int SearchTimeoutMs = 45'000;
    constexpr int ResolveTimeoutMs = 45'000;
    constexpr int ListStallTimeoutMs = 45'000;

    constexpr int MaxSearchResults = 100;
    constexpr int PageSize = 20;


    void StopProcess(QProcess* process)
    {
        if (!process)
            return;

        process->disconnect();
        process->kill();
        process->waitForFinished(1000);
        process->deleteLater();
    }

    QString LastErrorLine(const QByteArray& stderrOutput)
    {
        const QStringList lines = QString::fromLocal8Bit(stderrOutput).split('\n', Qt::SkipEmptyParts);
        for (auto it = lines.crbegin(); it != lines.crend(); ++it)
        {
            const QString line = it->trimmed();
            if (line.startsWith(QStringLiteral("ERROR:")))
                return line.mid(6).trimmed();
        }
        return lines.isEmpty() ? QString() : lines.last().trimmed();
    }
}

QString YtDlpBackend::GetAudioFormat()
{
    const QString quality = AppConfigs::GetString(AppConfigs::SettingsKeys::AudioQuality, AppConfigs::AudioQuality::High);
    if (quality == AppConfigs::AudioQuality::Best)
        return QStringLiteral("bestaudio/best");
    if (quality == AppConfigs::AudioQuality::Medium)
        return QStringLiteral("bestaudio[abr<=96]/worstaudio/bestaudio/best");
    return QStringLiteral("bestaudio[ext=m4a]/bestaudio/best");
}

QString YtDlpBackend::GetDownloadFormat()
{
    const QString format = AppConfigs::GetString(AppConfigs::SettingsKeys::DownloadFormat, AppConfigs::DownloadFormat::Auto);
    if (format == AppConfigs::DownloadFormat::Auto)
        return GetAudioFormat();

    const QString ext = format == AppConfigs::DownloadFormat::Opus ? QStringLiteral("webm") : QStringLiteral("m4a");
    const QString quality = AppConfigs::GetString(AppConfigs::SettingsKeys::AudioQuality, AppConfigs::AudioQuality::High);
    if (quality == AppConfigs::AudioQuality::Medium)
        return QStringLiteral("bestaudio[ext=%1][abr<=96]/worstaudio[ext=%1]/bestaudio/best").arg(ext);
    return QStringLiteral("bestaudio[ext=%1]/bestaudio/best").arg(ext);
}

QString YtDlpBackend::GetUnavailableReason() const
{
    return YtDlpSubsystem::Get().IsAvailable() ? QString() : QStringLiteral("yt-dlp is not installed yet");
}

void YtDlpBackend::Shutdown()
{
    StopProcess(searchProcess);
    for (const QPointer<QProcess>& process : streamProcesses)
        StopProcess(process);
    streamProcesses.clear();
    for (const QPointer<QProcess>& process : listProcesses)
        StopProcess(process);
    listProcesses.clear();
}

OnlineError YtDlpBackend::ClassifyError(const QString& message)
{
    const QString text = message.toLower();

    static const QStringList notFound{
        QStringLiteral("video unavailable"), QStringLiteral("private video"), QStringLiteral("has been removed"),
        QStringLiteral("does not exist"), QStringLiteral("video is not available"), QStringLiteral("account associated"),
        QStringLiteral("does not have a"),
    };
    static const QStringList blocked{
        QStringLiteral("not a bot"), QStringLiteral("sign in"), QStringLiteral("http error 403"),
        QStringLiteral("http error 429"), QStringLiteral("too many requests"), QStringLiteral("po token"),
        QStringLiteral("confirm your age"), QStringLiteral("age-restricted"),
    };

    for (const QString& marker : notFound)
    {
        if (text.contains(marker))
            return OnlineError::Make(OnlineError::NotFound, message);
    }
    for (const QString& marker : blocked)
    {
        if (text.contains(marker))
            return OnlineError::Make(OnlineError::Blocked, message);
    }
    return OnlineError::Make(OnlineError::Failed, message);
}

QProcess* YtDlpBackend::Run(const QStringList& args, int timeoutMs,
    std::function<void(const QByteArray& output, const OnlineError& error)> onDone,
    std::function<void(const QByteArray& line)> onLine)
{
    const QString executable = YtDlpSubsystem::Get().GetExecutablePath();
    if (executable.isEmpty())
    {
        YtDlpSubsystem::Get().EnsureLatest();
        QTimer::singleShot(0, this, [onDone] {
            onDone({}, OnlineError::Make(OnlineError::Unavailable, QStringLiteral("yt-dlp is not installed yet")));
        });
        return nullptr;
    }

    QStringList fullArgs = args;

    const QString deno = DenoSubsystem::Get().GetExecutablePath();
    if (!deno.isEmpty())
        fullArgs << QStringLiteral("--js-runtimes") << QStringLiteral("deno:") + deno;

    auto* process = new QProcess(this);
    auto* timeout = new QTimer(process);
    timeout->setSingleShot(true);

    auto finish = [process, onDone, bDone = std::make_shared<bool>(false)](const QByteArray& output, const OnlineError& error) {
        if (*bDone)
            return;
        *bDone = true;

        if (error.IsOk())
            YtDlpSubsystem::Get().ReportRunSuccess();
        else if (error.kind == OnlineError::Failed || error.kind == OnlineError::Blocked)
            YtDlpSubsystem::Get().ReportRunFailure();

        process->disconnect();
        process->deleteLater();
        onDone(output, error);
    };

    if (onLine)
    {
        connect(process, &QProcess::readyReadStandardOutput, process, [process, timeout, timeoutMs, onLine] {
            timeout->start(timeoutMs);
            while (process->canReadLine())
                onLine(process->readLine());
        });
    }

    connect(process, &QProcess::finished, process, [process, finish, onLine](int exitCode, QProcess::ExitStatus status) {
        QByteArray output = process->readAllStandardOutput();
        if (onLine)
        {
            for (const QByteArray& line : output.split('\n'))
            {
                if (!line.trimmed().isEmpty())
                    onLine(line);
            }
            output.clear();
        }

        if (status == QProcess::NormalExit && exitCode == 0)
        {
            finish(output, {});
            return;
        }

        QString message = LastErrorLine(process->readAllStandardError());
        if (message.isEmpty())
            message = QStringLiteral("yt-dlp exited with code %1").arg(exitCode);
        finish({}, ClassifyError(message));
    });

    connect(process, &QProcess::errorOccurred, process, [process, finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish({}, OnlineError::Make(OnlineError::Unavailable, QStringLiteral("Cannot start yt-dlp: ") + process->errorString()));
    });

    connect(timeout, &QTimer::timeout, process, [process, finish] {
        process->kill();
        finish({}, OnlineError::Make(OnlineError::Failed, QStringLiteral("yt-dlp did not respond in time")));
    });

    timeout->start(timeoutMs);
    process->start(executable, fullArgs);
    return process;
}

void YtDlpBackend::Search(const QString& query, int count, SearchCallback callback)
{
    lastQuery = query;
    fetched = 0;
    bExhausted = true;
    RunSearch(query, count, 0, std::move(callback));
}

bool YtDlpBackend::CanSearchMore() const
{
    return !bExhausted && fetched < MaxSearchResults;
}

void YtDlpBackend::SearchMore(SearchCallback callback)
{
    if (!CanSearchMore())
    {
        OnlineBackend::SearchMore(std::move(callback));
        return;
    }
    RunSearch(lastQuery, std::min(fetched + PageSize, MaxSearchResults), fetched, std::move(callback));
}

void YtDlpBackend::RunSearch(const QString& query, int count, int skip, SearchCallback callback)
{
    CancelSearch();

    const QStringList args{
        QStringLiteral("ytsearch%1:%2").arg(count).arg(query),
        QStringLiteral("--flat-playlist"),
        QStringLiteral("--dump-single-json"),
        QStringLiteral("--no-warnings"),
    };

    searchProcess = Run(args, SearchTimeoutMs, [this, count, skip, callback](const QByteArray& output, const OnlineError& error) {
        searchProcess = nullptr;
        if (!error.IsOk())
        {
            callback({}, error);
            return;
        }

        const QList<OnlineEntry> entries = ParseSearchResults(output);
        fetched = static_cast<int>(entries.size());
        bExhausted = entries.size() < count;
        callback(entries.mid(skip), {});
    });
}

void YtDlpBackend::CancelSearch()
{
    StopProcess(searchProcess);
    searchProcess = nullptr;
}

QList<OnlineEntry> YtDlpBackend::ParseSearchResults(const QByteArray& json)
{
    QList<OnlineEntry> entries;

    const QJsonArray array = QJsonDocument::fromJson(json).object().value("entries").toArray();
    for (const QJsonValue& value : array)
    {
        const QJsonObject object = value.toObject();

        const QString extractor = object.value("ie_key").toString();
        if (!extractor.isEmpty() && extractor != QStringLiteral("Youtube"))
            continue;
        if (object.value("live_status").toString() == QStringLiteral("is_live"))
            continue;

        const QString videoId = object.value("id").toString();
        OnlineEntry entry;
        entry.pageUrl = object.value("url").toString();
        if (entry.pageUrl.isEmpty() && !videoId.isEmpty())
            entry.pageUrl = QStringLiteral("https://www.youtube.com/watch?v=") + videoId;
        if (entry.pageUrl.isEmpty())
            continue;

        entry.title = object.value("title").toString();
        entry.channel = object.value("channel").toString();
        if (entry.channel.isEmpty())
            entry.channel = object.value("uploader").toString();
        entry.channelUrl = object.value("channel_url").toString();
        entry.durationMs = static_cast<qint64>(object.value("duration").toDouble() * 1000);

        if (!videoId.isEmpty())
            entry.thumbnailUrl = QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(videoId);
        else
            entry.thumbnailUrl = object.value("thumbnails").toArray().first().toObject().value("url").toString();

        entries.append(entry);
    }
    return entries;
}

bool YtDlpBackend::ParseListItem(const QJsonObject& object, OnlineListItem& item)
{
    const QString liveStatus = object.value("live_status").toString();
    if (liveStatus == QStringLiteral("is_live") || liveStatus == QStringLiteral("is_upcoming"))
        return false;

    const QString extractor = object.value("ie_key").toString();
    const QString id = object.value("id").toString();
    const QString title = object.value("title").toString();
    QString url = object.value("url").toString();

    if (extractor.isEmpty() || extractor == QStringLiteral("Youtube"))
    {
        if (id.isEmpty() || title == QStringLiteral("[Private video]") || title == QStringLiteral("[Deleted video]"))
            return false;

        item.kind = OnlineListItem::Kind::Video;
        url = QStringLiteral("https://www.youtube.com/watch?v=") + id;
        item.entry.thumbnailUrl = QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(id);
    }
    else if (extractor == QStringLiteral("YoutubeTab") && !url.isEmpty())
    {
        item.kind = url.contains(QStringLiteral("/playlist")) ? OnlineListItem::Kind::Playlist : OnlineListItem::Kind::Channel;
        item.entry.thumbnailUrl = object.value("thumbnails").toArray().first().toObject().value("url").toString();
        if (item.entry.thumbnailUrl.startsWith(QStringLiteral("//")))
            item.entry.thumbnailUrl.prepend(QStringLiteral("https:"));
    }
    else
    {
        return false;
    }

    const auto firstOf = [&object](std::initializer_list<const char*> keys) {
        for (const char* key : keys)
        {
            const QString value = object.value(QLatin1String(key)).toString();
            if (!value.isEmpty())
                return value;
        }
        return QString();
    };
    const auto channelPage = [](const QString& channelId) {
        return channelId.isEmpty() ? QString() : QStringLiteral("https://www.youtube.com/channel/") + channelId;
    };

    item.listTitle = object.value("playlist_title").toString();
    item.listChannel = firstOf({"playlist_channel", "playlist_uploader"});
    item.listChannelUrl = channelPage(object.value("playlist_channel_id").toString());
    item.followers = static_cast<qint64>(object.value("channel_follower_count").toDouble());

    item.entry.pageUrl = url;
    item.entry.title = title;
    item.entry.channel = firstOf({"channel", "uploader", "playlist_channel", "playlist_uploader"});
    item.entry.channelUrl = firstOf({"channel_url"});
    if (item.entry.channelUrl.isEmpty())
        item.entry.channelUrl = channelPage(firstOf({"channel_id", "playlist_channel_id"}));
    item.entry.durationMs = static_cast<qint64>(object.value("duration").toDouble() * 1000);
    return true;
}

QProcess* YtDlpBackend::StartListing(const QString& url, int limit, ListItemCallback onItem, ListDoneCallback onDone)
{
    const QStringList args{
        QStringLiteral("--flat-playlist"),
        QStringLiteral("--lazy-playlist"),
        QStringLiteral("--dump-json"),
        QStringLiteral("--playlist-end"), QString::number(limit),
        QStringLiteral("--no-warnings"),
        url,
    };

    QProcess* process = Run(args, ListStallTimeoutMs,
        [this, onDone](const QByteArray&, const OnlineError& error) {
            listProcesses.removeAll(nullptr);
            onDone(error);
        },
        [onItem](const QByteArray& line) {
            OnlineListItem item;
            const QJsonObject object = QJsonDocument::fromJson(line).object();
            if (!object.isEmpty() && ParseListItem(object, item))
                onItem(item);
        });

    if (process)
        listProcesses.append(process);
    return process;
}

void YtDlpBackend::StopListing(QProcess* process)
{
    listProcesses.removeAll(process);
    StopProcess(process);
}

void YtDlpBackend::FetchVideo(const QString& pageUrl, VideoCallback callback)
{
    const QStringList args{
        QStringLiteral("--dump-json"),
        QStringLiteral("--skip-download"),
        QStringLiteral("--no-playlist"),
        QStringLiteral("--ignore-no-formats-error"),
        QStringLiteral("--no-warnings"),
        pageUrl,
    };

    auto process = std::make_shared<QPointer<QProcess>>();
    *process = Run(args, ResolveTimeoutMs, [this, process, pageUrl, callback](const QByteArray& output, const OnlineError& error) {
        streamProcesses.removeAll(*process);

        if (!error.IsOk())
        {
            callback({}, error);
            return;
        }

        const QJsonObject object = QJsonDocument::fromJson(output).object();
        const QString id = object.value("id").toString();

        OnlineEntry entry;
        entry.pageUrl = pageUrl;
        entry.title = object.value("title").toString();
        entry.channel = object.value("channel").toString();
        if (entry.channel.isEmpty())
            entry.channel = object.value("uploader").toString();
        entry.channelUrl = object.value("channel_url").toString();
        entry.thumbnailUrl = QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(id);
        entry.durationMs = static_cast<qint64>(object.value("duration").toDouble() * 1000);

        if (id.isEmpty())
            callback({}, OnlineError::Make(OnlineError::Failed, QStringLiteral("yt-dlp found no video")));
        else
            callback(entry, {});
    });

    if (*process)
        streamProcesses.append(*process);
}

void YtDlpBackend::ResolveStream(const QString& pageUrl, StreamCallback callback)
{
    ResolveStreamWith(pageUrl, {}, std::move(callback));
}

void YtDlpBackend::ResolveStreamWith(const QString& pageUrl, const Credentials& credentials, StreamCallback callback)
{
    QStringList args{
        QStringLiteral("-f"), GetAudioFormat(),
        QStringLiteral("--get-url"),
        QStringLiteral("--no-playlist"),
        QStringLiteral("--no-warnings"),
    };

    if (!credentials.poToken.isEmpty())
    {
        QString extractorArgs = QStringLiteral("youtube:player_client=mweb;po_token=mweb.gvs+") + credentials.poToken;
        if (credentials.cookiesFile.isEmpty() && !credentials.visitorData.isEmpty())
            extractorArgs += QStringLiteral(";player_skip=webpage,configs;visitor_data=") + credentials.visitorData;
        args << QStringLiteral("--extractor-args") << extractorArgs;
    }
    if (!credentials.cookiesFile.isEmpty())
        args << QStringLiteral("--cookies") << credentials.cookiesFile;

    args << pageUrl;

    auto process = std::make_shared<QPointer<QProcess>>();
    *process = Run(args, ResolveTimeoutMs, [this, process, callback](const QByteArray& output, const OnlineError& error) {
        streamProcesses.removeAll(*process);

        if (!error.IsOk())
        {
            callback({}, error);
            return;
        }

        const QUrl stream(QString::fromUtf8(output).section('\n', 0, 0).trimmed());
        if (!stream.isValid() || stream.scheme().isEmpty())
        {
            callback({}, OnlineError::Make(OnlineError::Failed, QStringLiteral("yt-dlp found no audio stream")));
            return;
        }
        callback(stream, {});
    });

    if (*process)
        streamProcesses.append(*process);
}
