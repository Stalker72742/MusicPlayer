//
// Created by Stalker7274 on 05.11.2026.
//

#include "LibraryStorage.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
    constexpr quint32 Magic = 0x424C4C53;
    constexpr quint32 FormatVersion = 3;
    constexpr int JsonVersion = 1;

    constexpr quint8 FlagMissing = 1 << 0;
    constexpr quint8 FlagTagsFromFile = 1 << 1;
    constexpr quint8 FlagLiked = 1 << 2;

    qint64 ToMs(const QDateTime& time)
    {
        return time.isValid() ? time.toMSecsSinceEpoch() : 0;
    }

    QDateTime FromMs(qint64 ms)
    {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime();
    }

    void PrepareStream(QDataStream& stream)
    {
        stream.setVersion(QDataStream::Qt_6_8);
        stream.setByteOrder(QDataStream::LittleEndian);
    }

    void WriteTrack(QDataStream& out, const Track& track)
    {
        quint8 flags = 0;
        flags |= track.file.bMissing ? FlagMissing : 0;
        flags |= track.tags.bFromFile ? FlagTagsFromFile : 0;
        flags |= track.stats.bLiked ? FlagLiked : 0;

        out << track.id.value << flags;
        out << track.file.path << track.file.size << track.file.modifiedMs << track.file.fileId << track.file.volumeId;
        out << track.tags.title << track.tags.artist << track.tags.album << track.tags.genre;
        out << static_cast<qint32>(track.tags.trackNumber) << static_cast<qint32>(track.tags.year) << track.tags.durationMs;
        out << ToMs(track.stats.dateAdded) << ToMs(track.stats.lastPlayed) << static_cast<qint32>(track.stats.playCount);
        out << track.user.labels;
        out << track.stream.pageUrl << track.stream.thumbnailUrl;
        out << track.stream.channelUrl;
        out << track.user.tagEdits;
    }

    void ReadTrack(QDataStream& in, Track& track, quint32 version)
    {
        quint8 flags = 0;
        qint32 trackNumber = 0;
        qint32 year = 0;
        qint32 playCount = 0;
        qint64 dateAdded = 0;
        qint64 lastPlayed = 0;

        in >> track.id.value >> flags;
        in >> track.file.path >> track.file.size >> track.file.modifiedMs >> track.file.fileId >> track.file.volumeId;
        in >> track.tags.title >> track.tags.artist >> track.tags.album >> track.tags.genre;
        in >> trackNumber >> year >> track.tags.durationMs;
        in >> dateAdded >> lastPlayed >> playCount;
        in >> track.user.labels;
        in >> track.stream.pageUrl >> track.stream.thumbnailUrl;
        if (version >= 2)
            in >> track.stream.channelUrl;
        if (version >= 3)
            in >> track.user.tagEdits;

        track.file.bMissing = flags & FlagMissing;
        track.tags.bFromFile = flags & FlagTagsFromFile;
        track.stats.bLiked = flags & FlagLiked;
        track.tags.trackNumber = trackNumber;
        track.tags.year = year;
        track.stats.playCount = playCount;
        track.stats.dateAdded = FromMs(dateAdded);
        track.stats.lastPlayed = FromMs(lastPlayed);
        track.stream.bSaved = track.IsOnline();
    }
}

LibraryStorage::LoadResult LibraryStorage::Load(const QString& path, QList<Track>& tracks)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QFile::exists(path) ? LoadResult::Corrupted : LoadResult::NoFile;

    const QByteArray bytes = file.readAll();

    QDataStream in(bytes);
    PrepareStream(in);

    quint32 magic = 0;
    quint32 version = 0;
    quint32 count = 0;
    in >> magic >> version >> count;
    if (in.status() != QDataStream::Ok || magic != Magic || version < 1 || version > FormatVersion)
        return LoadResult::Corrupted;

    QList<Track> loaded;
    loaded.reserve(std::min<quint32>(count, static_cast<quint32>(bytes.size() / 32)));
    for (quint32 i = 0; i < count; ++i)
    {
        Track track;
        ReadTrack(in, track, version);
        if (in.status() != QDataStream::Ok)
            return LoadResult::Corrupted;
        loaded.append(std::move(track));
    }

    tracks = std::move(loaded);
    return LoadResult::Loaded;
}

bool LibraryStorage::Save(const QString& path, const QList<const Track*>& tracks)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "[Library] Cannot save the registry:" << file.errorString();
        return false;
    }

    QDataStream out(&file);
    PrepareStream(out);

    out << Magic << FormatVersion << static_cast<quint32>(tracks.size());
    for (const Track* track : tracks)
        WriteTrack(out, *track);

    if (out.status() != QDataStream::Ok || !file.commit())
    {
        qWarning() << "[Library] Cannot save the registry:" << file.errorString();
        return false;
    }
    return true;
}

bool LibraryStorage::ImportJson(const QString& path, QList<Track>& tracks)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value("version").toInt() != JsonVersion)
    {
        qWarning() << "[Library] Ignoring a cache of another version:" << path;
        return false;
    }

    for (const QJsonValue& value : root.value("tracks").toArray())
    {
        const QJsonObject object = value.toObject();

        Track track;
        track.id = TrackId{static_cast<quint32>(object.value("id").toInteger())};
        track.file.path = object.value("path").toString();
        track.file.size = object.value("size").toInteger();
        track.file.modifiedMs = object.value("modified").toInteger();

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

        tracks.append(std::move(track));
    }
    return true;
}
