//
// Created by Stalker7274 on 08.11.2026.
//

#include "Catalog/ArtworkSubsystem.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>

#include <algorithm>

#include "AppConfigs.h"

namespace
{
    constexpr qint64 NotFoundRetryMs = 30LL * 24 * 60 * 60 * 1000;
    constexpr int RequestSpacingMs = 250;
    constexpr int RescanDelayMs = 300;
    constexpr int SaveDelayMs = 2000;
    constexpr int TimeoutMs = 15000;

    const QStringList ImageExtensions{QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("webp"), QStringLiteral("svg"), QStringLiteral("gif"), QStringLiteral("bmp")};

    int Slot(CatalogKind kind)
    {
        return static_cast<int>(kind);
    }

    QString KindFolderName(CatalogKind kind)
    {
        switch (kind)
        {
            case CatalogKind::Artist: return QStringLiteral("Artists");
            case CatalogKind::Album:  return QStringLiteral("Albums");
            case CatalogKind::Genre:  return QStringLiteral("Genres");
        }
        return {};
    }

    /// Letters and digits only, case folded: "Rap/Hip Hop" and "rap hip-hop" match.
    QString Normalize(const QString& text)
    {
        QString result;
        for (const QChar c : text.toCaseFolded())
        {
            if (c.isLetterOrNumber())
                result.append(c);
        }
        return result;
    }

    /// Deezer answers with an image URL even when it has no picture; such URLs have an empty hash.
    bool IsRealPicture(const QString& url)
    {
        return !url.isEmpty() && !url.contains(QLatin1String("//1000x1000")) && !url.contains(QLatin1String("//500x500"));
    }

    void MoveAway(const QString& path)
    {
        QFile file(path);
        if (!file.moveToTrash())
            file.remove();
    }
}

ArtworkSubsystem::ArtworkSubsystem()
{
    watcher = new QFileSystemWatcher(this);
    rescanTimer.setSingleShot(true);
    rescanTimer.setInterval(RescanDelayMs);
    connect(&rescanTimer, &QTimer::timeout, this, &ArtworkSubsystem::RescanAll);
    connect(watcher, &QFileSystemWatcher::directoryChanged, &rescanTimer, qOverload<>(&QTimer::start));

    spacingTimer.setSingleShot(true);
    spacingTimer.setInterval(RequestSpacingMs);
    connect(&spacingTimer, &QTimer::timeout, this, &ArtworkSubsystem::StartNext);

    saveTimer.setSingleShot(true);
    saveTimer.setInterval(SaveDelayMs);
    connect(&saveTimer, &QTimer::timeout, this, &ArtworkSubsystem::SaveLookups);

    LoadLookups();
    WatchFolders();
    RescanAll();
}

void ArtworkSubsystem::Deinitialize()
{
    queue.clear();
    if (currentReply)
    {
        currentReply->abort();
        currentReply = nullptr;
    }
    delete network;
    network = nullptr;

    if (saveTimer.isActive())
    {
        saveTimer.stop();
        SaveLookups();
    }
}

QString ArtworkSubsystem::GetFolder() const
{
    return QDir::cleanPath(AppConfigs::GetString(AppConfigs::SettingsKeys::ArtworkFolder));
}

QString ArtworkSubsystem::GetKindFolder(CatalogKind kind) const
{
    const QString folder = GetFolder() + QLatin1Char('/') + KindFolderName(kind);
    QDir().mkpath(folder);
    return folder;
}

QString ArtworkSubsystem::FindImage(CatalogKind kind, const QString& imageName) const
{
    return files[Slot(kind)].value(imageName.toCaseFolded());
}

QUrl ArtworkSubsystem::GetImageUrl(CatalogKind kind, const QString& imageName) const
{
    const QString path = FindImage(kind, imageName);
    if (path.isEmpty())
        return {};

    QUrl url = QUrl::fromLocalFile(path);
    url.setQuery(QStringLiteral("v=%1").arg(QFileInfo(path).lastModified().toMSecsSinceEpoch()));
    return url;
}

QString ArtworkSubsystem::LookupKey(CatalogKind kind, const QString& imageName)
{
    return CatalogKindId(kind) + QLatin1Char('/') + imageName.toCaseFolded();
}

void ArtworkSubsystem::Request(const CatalogEntry& entry)
{
    if (entry.imageName.isEmpty() || !FindImage(entry.kind, entry.imageName).isEmpty()
        || !AppConfigs::GetBool(AppConfigs::SettingsKeys::FetchArtwork, true))
        return;

    const QString key = LookupKey(entry.kind, entry.imageName);
    if (failedThisRun.contains(key) || IsFetching(entry.kind, entry.imageName))
        return;

    const qint64 triedAt = notFoundAt.value(key, 0);
    if (triedAt > 0 && QDateTime::currentMSecsSinceEpoch() - triedAt < NotFoundRetryMs)
        return;

    Enqueue(entry, false);
}

void ArtworkSubsystem::FindOnline(const CatalogEntry& entry)
{
    if (entry.imageName.isEmpty() || IsFetching(entry.kind, entry.imageName))
        return;

    const QString key = LookupKey(entry.kind, entry.imageName);
    failedThisRun.remove(key);
    Enqueue(entry, true);
}

bool ArtworkSubsystem::IsFetching(CatalogKind kind, const QString& imageName) const
{
    const auto same = [&](const Job& job) {
        return job.kind == kind && job.imageName.compare(imageName, Qt::CaseInsensitive) == 0;
    };
    return (current && same(*current)) || std::any_of(queue.cbegin(), queue.cend(), same);
}

void ArtworkSubsystem::Enqueue(const CatalogEntry& entry, bool bForced)
{
    Job job{entry.kind, entry.imageName, entry.name, entry.artist, bForced};
    if (bForced)
        queue.prepend(job);
    else
        queue.append(job);

    if (!current && !spacingTimer.isActive())
        StartNext();
}

void ArtworkSubsystem::StartNext()
{
    if (current || queue.isEmpty())
        return;

    current = queue.takeFirst();

    if (!current->bForced && !FindImage(current->kind, current->imageName).isEmpty())
    {
        current.reset();
        StartNext();
        return;
    }

    SearchDeezer();
}

QNetworkReply* ArtworkSubsystem::SendGet(const QUrl& url)
{
    if (!network)
        network = new QNetworkAccessManager(this);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QCoreApplication::applicationName() + QLatin1Char('/') + QCoreApplication::applicationVersion());
    request.setTransferTimeout(TimeoutMs);
    // Genre names are localized by region otherwise, and the tags are usually English.
    request.setRawHeader("Accept-Language", "en");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    currentReply = network->get(request);
    return currentReply;
}

void ArtworkSubsystem::SearchDeezer()
{
    const Job job = *current;

    if (job.kind == CatalogKind::Genre && bGenresLoaded)
    {
        const QUrl url = deezerGenres.value(Normalize(job.name));
        if (url.isValid())
            DownloadImage(url);
        else
            FinishJob(false, false);
        return;
    }

    QUrl url;
    QUrlQuery query;
    switch (job.kind)
    {
        case CatalogKind::Artist:
            url = QUrl(QStringLiteral("https://api.deezer.com/search/artist"));
            query.addQueryItem(QStringLiteral("q"), job.name);
            query.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
            break;
        case CatalogKind::Album:
        {
            url = QUrl(QStringLiteral("https://api.deezer.com/search/album"));
            const bool bVarious = job.artist.isEmpty() || job.artist == QLatin1String("Various artists");
            query.addQueryItem(QStringLiteral("q"), bVarious ? job.name : job.artist + QLatin1Char(' ') + job.name);
            query.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
            break;
        }
        case CatalogKind::Genre:
            url = QUrl(QStringLiteral("https://api.deezer.com/genre"));
            break;
    }
    url.setQuery(query);

    QNetworkReply* reply = SendGet(url);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply != currentReply)
            return;
        currentReply = nullptr;

        if (reply->error() != QNetworkReply::NoError)
        {
            qWarning() << "[Artwork] Lookup failed:" << reply->errorString();
            FinishJob(false, true);
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (root.contains(QStringLiteral("error")))
        {
            qWarning() << "[Artwork] Deezer error:" << root.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
            FinishJob(false, true);
            return;
        }

        const QJsonArray data = root.value(QStringLiteral("data")).toArray();
        const Job job = *current;
        const QString wanted = Normalize(job.name);
        QString picture;

        if (job.kind == CatalogKind::Genre)
        {
            for (const QJsonValue& value : data)
            {
                const QJsonObject genre = value.toObject();
                const QString url = genre.value(QStringLiteral("picture_xl")).toString();
                if (genre.value(QStringLiteral("id")).toInt() != 0 && IsRealPicture(url))
                    deezerGenres.insert(Normalize(genre.value(QStringLiteral("name")).toString()), QUrl(url));
            }
            bGenresLoaded = true;
            picture = deezerGenres.value(wanted).toString();
        }
        else if (job.kind == CatalogKind::Artist)
        {
            for (const QJsonValue& value : data)
            {
                const QJsonObject artist = value.toObject();
                if (Normalize(artist.value(QStringLiteral("name")).toString()) == wanted)
                {
                    picture = artist.value(QStringLiteral("picture_xl")).toString();
                    break;
                }
            }
        }
        else
        {
            const QString wantedArtist = Normalize(job.artist);
            QString loose;
            for (const QJsonValue& value : data)
            {
                const QJsonObject album = value.toObject();
                const QString title = Normalize(album.value(QStringLiteral("title")).toString());
                const QString artist = Normalize(album.value(QStringLiteral("artist")).toObject().value(QStringLiteral("name")).toString());
                const QString cover = album.value(QStringLiteral("cover_xl")).toString();
                const bool bArtistOk = job.artist == QLatin1String("Various artists") || artist == wantedArtist;
                if (title == wanted && bArtistOk)
                {
                    picture = cover;
                    break;
                }
                if (loose.isEmpty() && bArtistOk && (title.startsWith(wanted) || wanted.startsWith(title)) && !title.isEmpty())
                    loose = cover;
            }
            if (picture.isEmpty())
                picture = loose;
        }

        if (IsRealPicture(picture))
            DownloadImage(QUrl(picture));
        else
            FinishJob(false, false);
    });
}

void ArtworkSubsystem::DownloadImage(const QUrl& url)
{
    QNetworkReply* reply = SendGet(url);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply != currentReply)
            return;
        currentReply = nullptr;

        const QByteArray bytes = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || bytes.isEmpty())
        {
            qWarning() << "[Artwork] Download failed:" << reply->errorString();
            FinishJob(false, true);
            return;
        }

        const Job job = *current;
        const QString folder = GetKindFolder(job.kind);
        const QString target = folder + QLatin1Char('/') + job.imageName + QStringLiteral(".jpg");

        const QString existing = FindImage(job.kind, job.imageName);
        if (!existing.isEmpty() && QFileInfo(existing) != QFileInfo(target))
            MoveAway(existing);

        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            qWarning() << "[Artwork] Cannot write" << target << file.errorString();
            FinishJob(false, true);
            return;
        }

        RescanFolder(job.kind);
        emit imagesChanged();
        FinishJob(true, false);
    });
}

void ArtworkSubsystem::FinishJob(bool bFound, bool bNetworkError)
{
    const Job job = *current;
    current.reset();

    const QString key = LookupKey(job.kind, job.imageName);
    if (bNetworkError)
        failedThisRun.insert(key);
    else if (!bFound)
    {
        notFoundAt.insert(key, QDateTime::currentMSecsSinceEpoch());
        saveTimer.start();
    }
    else if (notFoundAt.remove(key) > 0)
        saveTimer.start();

    emit lookupFinished(job.kind, job.imageName, bFound);
    spacingTimer.start();
}

QString ArtworkSubsystem::SetImage(CatalogKind kind, const QString& imageName, const QString& sourcePath)
{
    const QFileInfo source(sourcePath);
    const QString extension = source.suffix().toLower();
    if (!source.isFile() || !ImageExtensions.contains(extension))
        return QStringLiteral("Pick a JPG, PNG, WebP, SVG, GIF or BMP image.");

    const QString target = GetKindFolder(kind) + QLatin1Char('/') + imageName + QLatin1Char('.') + extension;
    if (QFileInfo(target) == source)
        return {};

    const QString existing = FindImage(kind, imageName);
    if (!existing.isEmpty())
        MoveAway(existing);
    if (QFile::exists(target))
        MoveAway(target);

    if (!QFile::copy(sourcePath, target))
        return QStringLiteral("Cannot copy the image into ") + QDir::toNativeSeparators(GetKindFolder(kind));

    notFoundAt.remove(LookupKey(kind, imageName));
    saveTimer.start();
    RescanFolder(kind);
    emit imagesChanged();
    return {};
}

void ArtworkSubsystem::RemoveImage(CatalogKind kind, const QString& imageName)
{
    const QString existing = FindImage(kind, imageName);
    if (existing.isEmpty())
        return;

    MoveAway(existing);
    notFoundAt.insert(LookupKey(kind, imageName), QDateTime::currentMSecsSinceEpoch());
    saveTimer.start();
    RescanFolder(kind);
    emit imagesChanged();
}

void ArtworkSubsystem::ApplyFolderSetting()
{
    WatchFolders();
    RescanAll();
}

void ArtworkSubsystem::WatchFolders()
{
    if (!watcher->directories().isEmpty())
        watcher->removePaths(watcher->directories());

    for (CatalogKind kind : {CatalogKind::Artist, CatalogKind::Album, CatalogKind::Genre})
        watcher->addPath(GetKindFolder(kind));
}

void ArtworkSubsystem::RescanFolder(CatalogKind kind)
{
    QHash<QString, QString> found;
    const QFileInfoList entries = QDir(GetKindFolder(kind)).entryInfoList(QDir::Files | QDir::Readable);
    for (const QFileInfo& info : entries)
    {
        if (ImageExtensions.contains(info.suffix().toLower()))
            found.insert(info.completeBaseName().toCaseFolded(), info.absoluteFilePath());
    }
    files[Slot(kind)] = found;
}

void ArtworkSubsystem::RescanAll()
{
    for (CatalogKind kind : {CatalogKind::Artist, CatalogKind::Album, CatalogKind::Genre})
        RescanFolder(kind);

    // A replaced file keeps its name; its new time changes the URL, so QML loads it again.
    emit imagesChanged();
}

void ArtworkSubsystem::LoadLookups()
{
    QFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/artwork-lookups.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = root.begin(); it != root.end(); ++it)
        notFoundAt.insert(it.key(), static_cast<qint64>(it.value().toDouble()));
}

void ArtworkSubsystem::SaveLookups() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QJsonObject root;
    for (auto it = notFoundAt.cbegin(); it != notFoundAt.cend(); ++it)
    {
        if (now - it.value() < NotFoundRetryMs)
            root.insert(it.key(), static_cast<double>(it.value()));
    }

    QSaveFile file(dir + QStringLiteral("/artwork-lookups.json"));
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
