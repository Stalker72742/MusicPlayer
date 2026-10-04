//
// Created by Stalker7274 on 03.10.2026.
//

#include "MetadataReader.h"

#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QUrl>

namespace
{
    constexpr int ReadTimeoutMs = 5000;
}

MetadataReader::MetadataReader(QObject* parent)
    : QObject(parent)
    , player(new QMediaPlayer(this))
{
    timeout.setSingleShot(true);
    timeout.setInterval(ReadTimeoutMs);
    connect(&timeout, &QTimer::timeout, this, [this] { Finish(false); });

    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (!current.IsValid())
            return;

        if (status == QMediaPlayer::LoadedMedia)
            Finish(true);
        else if (status == QMediaPlayer::InvalidMedia)
            Finish(false);
    });
}

MetadataReader::~MetadataReader()
{
    timeout.stop();
}

void MetadataReader::Enqueue(TrackId id, const QString& path)
{
    queue.append({id, path});

    if (!current.IsValid())
        StartNext();
}

void MetadataReader::Clear()
{
    queue.clear();
}

void MetadataReader::StartNext()
{
    if (current.IsValid())
        return;

    if (queue.isEmpty())
    {
        current = {};
        player->setSource({});
        emit idle();
        return;
    }

    const Request request = queue.takeFirst();
    current = request.id;
    timeout.start();
    player->setSource(QUrl::fromLocalFile(request.path));
}

void MetadataReader::Finish(bool bSuccess)
{
    timeout.stop();

    TagsComponent tags;
    if (bSuccess)
    {
        const QMediaMetaData metaData = player->metaData();

        tags.title = metaData.stringValue(QMediaMetaData::Title);
        tags.artist = metaData.stringValue(QMediaMetaData::ContributingArtist);
        if (tags.artist.isEmpty())
            tags.artist = metaData.stringValue(QMediaMetaData::AlbumArtist);
        tags.album = metaData.stringValue(QMediaMetaData::AlbumTitle);
        tags.genre = metaData.stringValue(QMediaMetaData::Genre);
        tags.trackNumber = metaData.value(QMediaMetaData::TrackNumber).toInt();
        tags.year = metaData.value(QMediaMetaData::Date).toDateTime().date().year();
        tags.durationMs = player->duration();
        tags.bFromFile = true;
    }

    const TrackId id = current;
    current = {};
    emit tagsRead(id, tags, bSuccess);

    QMetaObject::invokeMethod(this, &MetadataReader::StartNext, Qt::QueuedConnection);
}
