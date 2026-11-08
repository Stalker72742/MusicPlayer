//
// Created by Stalker7274 on 03.10.2026.
//

#include "MetadataReader.h"

#include <QMetaObject>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>
}

namespace
{
    constexpr int MaxThreads = 4;

    QString FindTag(const AVDictionary* container, const AVDictionary* stream, const char* key)
    {
        const AVDictionaryEntry* entry = av_dict_get(container, key, nullptr, 0);
        if (!entry)
            entry = av_dict_get(stream, key, nullptr, 0);
        return entry ? QString::fromUtf8(entry->value).trimmed() : QString();
    }

    bool ReadTags(const QString& path, TagsComponent& tags)
    {
        AVFormatContext* container = nullptr;
        if (avformat_open_input(&container, path.toUtf8().constData(), nullptr, nullptr) < 0)
            return false;

        const bool bInfo = avformat_find_stream_info(container, nullptr) >= 0;
        const int index = bInfo ? av_find_best_stream(container, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0) : -1;
        if (index < 0)
        {
            avformat_close_input(&container);
            return false;
        }

        const AVDictionary* fromFile = container->metadata;
        const AVDictionary* fromStream = container->streams[index]->metadata;

        tags.title = FindTag(fromFile, fromStream, "title");
        tags.artist = FindTag(fromFile, fromStream, "artist");
        if (tags.artist.isEmpty())
            tags.artist = FindTag(fromFile, fromStream, "album_artist");
        tags.album = FindTag(fromFile, fromStream, "album");
        tags.genre = FindTag(fromFile, fromStream, "genre");
        tags.trackNumber = FindTag(fromFile, fromStream, "track").section(QLatin1Char('/'), 0, 0).toInt();
        tags.year = FindTag(fromFile, fromStream, "date").left(4).toInt();
        if (container->duration > 0)
            tags.durationMs = container->duration / (AV_TIME_BASE / 1000);
        tags.bFromFile = true;

        avformat_close_input(&container);
        return true;
    }
}

MetadataReader::MetadataReader(QObject* parent)
    : QObject(parent)
{
    av_log_set_level(AV_LOG_QUIET);
    pool.setMaxThreadCount(MaxThreads);
}

MetadataReader::~MetadataReader()
{
    queue.clear();
    pool.waitForDone();
}

void MetadataReader::Enqueue(TrackId id, const QString& path)
{
    if (!queued.contains(id))
    {
        queued.insert(id);
        queue.append({id, path});
    }
    StartNext();
}

void MetadataReader::Clear()
{
    queue.clear();
    queued.clear();
}

void MetadataReader::StartNext()
{
    while (running < MaxThreads && !queue.isEmpty())
    {
        const Request request = queue.takeFirst();
        queued.remove(request.id);
        ++running;

        pool.start([this, request] {
            TagsComponent tags;
            const bool bSuccess = ReadTags(request.path, tags);
            QMetaObject::invokeMethod(this, [this, id = request.id, tags, bSuccess] { OnRead(id, tags, bSuccess); }, Qt::QueuedConnection);
        });
    }
}

void MetadataReader::OnRead(TrackId id, const TagsComponent& tags, bool bSuccess)
{
    --running;
    emit tagsRead(id, tags, bSuccess);
    StartNext();

    if (running == 0 && queue.isEmpty())
        emit idle();
}
