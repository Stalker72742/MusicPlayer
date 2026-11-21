//
// Created by Stalker7274 on 20.11.2026.
//

#include "SyncEngine.h"

#include "Library/LibrarySubsystem.h"
#include "Playlists/PlaylistSubsystem.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <optional>
#include <utility>

namespace
{
    constexpr qsizetype MassVanishThreshold = 20;
    constexpr qsizetype PlaylistVanishThreshold = 2;
    constexpr qsizetype DigestSize = 32;
    constexpr qsizetype MetaIdentitySize = 12;

    QByteArray Digest(const QByteArray& bytes)
    {
        return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    }

    bool SamePlaylist(const Sync::PlaylistBody& a, const Sync::PlaylistBody& b)
    {
        return a.name == b.name && a.bSmart == b.bSmart && a.tracks == b.tracks && a.smart == b.smart &&
               a.extras == b.extras;
    }

    const Track* OnlineTrackOfKey(const Track* track, const QString& key)
    {
        if (!track || !track->IsOnline())
        {
            return nullptr;
        }
        return Sync::MakeKey(Sync::Kind::Online, Sync::OnlineUid(track->stream.pageUrl)) == key ? track : nullptr;
    }

    QByteArray OnlineDigest(const Track* track)
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        if (track && track->IsOnline() && track->stream.bSaved)
        {
            stream << quint8(1) << track->stream.pageUrl << track->stream.thumbnailUrl << track->stream.channelUrl;
        }
        else
        {
            stream << quint8(0);
        }
        return Digest(bytes);
    }

    QByteArray MetaDigest(const QStringList& labels, const QMap<QString, QString>& tagEdits)
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        stream << labels << tagEdits;
        return Digest(bytes);
    }

    QByteArray MetaSeen(const Track& track)
    {
        QByteArray identity;
        QDataStream stream(&identity, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        const qint64 addedMs = track.stats.dateAdded.isValid() ? track.stats.dateAdded.toMSecsSinceEpoch() : 0;
        stream << track.id.value << addedMs;
        return identity + MetaDigest(track.user.labels, track.user.tagEdits);
    }

    QByteArray PlaylistDigest(const Playlist& playlist)
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        const qint64 createdMs = playlist.created.isValid() ? playlist.created.toMSecsSinceEpoch() : 0;
        stream << playlist.name << static_cast<qint32>(playlist.kind) << createdMs;
        stream << quint32(playlist.tracks.size());
        for (const TrackId id : playlist.tracks)
        {
            stream << id.value;
        }
        stream << quint32(playlist.rules.size());
        for (const PlaylistRule& rule : playlist.rules)
        {
            stream << rule.field << rule.op << rule.value;
        }
        stream << playlist.bMatchAll << playlist.sort << qint32(playlist.limit);
        stream << quint32(playlist.excluded.size());
        for (const TrackId id : playlist.excluded)
        {
            stream << id.value;
        }
        return Digest(bytes);
    }

    QByteArray PackSeen(const QByteArray& digest, const QHash<quint32, QString>& members)
    {
        QList<quint32> ids = members.keys();
        std::sort(ids.begin(), ids.end());
        QByteArray tail;
        QDataStream stream(&tail, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        stream << quint32(ids.size());
        for (const quint32 id : ids)
        {
            stream << id << members.value(id);
        }
        return digest + tail;
    }

    bool ParseSeen(const QByteArray& seen, QByteArray& digest, QHash<quint32, QString>& members)
    {
        digest.clear();
        members.clear();
        if (seen.size() < DigestSize)
        {
            return false;
        }
        QDataStream stream(seen.mid(DigestSize));
        stream.setVersion(QDataStream::Qt_6_5);
        quint32 count = 0;
        stream >> count;
        for (quint32 i = 0; i < count && stream.status() == QDataStream::Ok; ++i)
        {
            quint32 id = 0;
            QString uid;
            stream >> id >> uid;
            members.insert(id, uid);
        }
        if (stream.status() != QDataStream::Ok)
        {
            members.clear();
            return false;
        }
        digest = seen.left(DigestSize);
        return true;
    }

    bool InStaticPlaylist(TrackId id)
    {
        const QList<Playlist>& playlists = PlaylistSubsystem::Get().GetPlaylists();
        return std::any_of(playlists.cbegin(), playlists.cend(), [id](const Playlist& playlist)
        {
            return !playlist.IsSmart() && playlist.tracks.contains(id);
        });
    }

    Sync::Record OnlineRecordOf(const QString& uid, const Track& track, const Sync::TrackInfo& info)
    {
        Sync::Record record;
        record.kind = Sync::Kind::Online;
        record.uid = uid;
        record.online.pageUrl = track.stream.pageUrl;
        record.online.info = info;
        record.online.thumbnailUrl = track.stream.thumbnailUrl;
        record.online.channelUrl = track.stream.channelUrl;
        record.online.bSaved = true;
        return record;
    }

    bool FillOnline(Sync::Record& record, const Track& track, const Sync::TrackInfo& info)
    {
        Sync::OnlineBody& body = record.online;
        bool bChanged = false;
        if (body.pageUrl.isEmpty() && !track.stream.pageUrl.isEmpty())
        {
            body.pageUrl = track.stream.pageUrl;
            bChanged = true;
        }
        if (body.info.title.isEmpty() && !info.title.isEmpty())
        {
            body.info = info;
            bChanged = true;
        }
        if (body.thumbnailUrl.isEmpty() && !track.stream.thumbnailUrl.isEmpty())
        {
            body.thumbnailUrl = track.stream.thumbnailUrl;
            bChanged = true;
        }
        if (body.channelUrl.isEmpty() && !track.stream.channelUrl.isEmpty())
        {
            body.channelUrl = track.stream.channelUrl;
            bChanged = true;
        }
        if (!body.bSaved)
        {
            body.bSaved = true;
            bChanged = true;
        }
        return bChanged;
    }

    struct SmartDefinition
    {
        QList<PlaylistRule> rules;
        bool bMatchAll = true;
        QString sort = QStringLiteral("added");
        int limit = 0;
        QStringList excluded;
    };

    QString JsonText(const QJsonValue& value)
    {
        if (value.isString())
        {
            return value.toString();
        }
        if (value.isBool())
        {
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        if (value.isDouble())
        {
            return QString::number(value.toDouble());
        }
        return {};
    }

    std::optional<SmartDefinition> ParseSmart(const QString& json)
    {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            return std::nullopt;
        }
        const QJsonObject object = document.object();
        SmartDefinition definition;
        const QJsonArray rules = object.value(QStringLiteral("rules")).toArray();
        for (const QJsonValue& value : rules)
        {
            const QJsonObject rule = value.toObject();
            definition.rules.append(PlaylistRule{JsonText(rule.value(QStringLiteral("field"))),
                                                 JsonText(rule.value(QStringLiteral("op"))),
                                                 JsonText(rule.value(QStringLiteral("value")))});
        }
        const QJsonValue matchAll = object.value(QStringLiteral("matchAll"));
        if (matchAll.isBool())
        {
            definition.bMatchAll = matchAll.toBool();
        }
        else if (matchAll.isString())
        {
            definition.bMatchAll = matchAll.toString() != QStringLiteral("false");
        }
        const QString sort = object.value(QStringLiteral("sort")).toString();
        if (!sort.isEmpty())
        {
            definition.sort = sort;
        }
        const QJsonValue limit = object.value(QStringLiteral("limit"));
        if (limit.isDouble())
        {
            definition.limit = std::max(0, limit.toInt());
        }
        else if (limit.isString())
        {
            definition.limit = std::max(0, limit.toString().toInt());
        }
        QSet<QString> unique;
        const QJsonArray excluded = object.value(QStringLiteral("excluded")).toArray();
        for (const QJsonValue& value : excluded)
        {
            const QString uid = value.toString();
            if (Sync::IsUid(uid) && !unique.contains(uid))
            {
                unique.insert(uid);
                definition.excluded.append(uid);
            }
        }
        return definition;
    }

    QString BuildSmart(const Playlist& playlist, const QStringList& excluded)
    {
        QJsonArray rules;
        for (const PlaylistRule& rule : playlist.rules)
        {
            QJsonObject object;
            object.insert(QStringLiteral("field"), rule.field);
            object.insert(QStringLiteral("op"), rule.op);
            object.insert(QStringLiteral("value"), rule.value);
            rules.append(object);
        }
        QJsonObject object;
        object.insert(QStringLiteral("rules"), rules);
        object.insert(QStringLiteral("matchAll"), playlist.bMatchAll);
        object.insert(QStringLiteral("sort"), playlist.sort);
        object.insert(QStringLiteral("limit"), playlist.limit);
        object.insert(QStringLiteral("excluded"), QJsonArray::fromStringList(excluded));
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    template <typename T>
    QList<T> Interleave(const QList<T>& base, const QList<T>& reference, const QList<T>& extras, const T& start)
    {
        if (extras.isEmpty())
        {
            return base;
        }
        const QSet<T> kept(base.cbegin(), base.cend());
        const QSet<T> extraSet(extras.cbegin(), extras.cend());
        QHash<T, QList<T>> followers;
        T anchor = start;
        for (const T& item : reference)
        {
            if (extraSet.contains(item))
            {
                followers[anchor].append(item);
                anchor = item;
            }
            else if (kept.contains(item))
            {
                anchor = item;
            }
        }

        QList<T> result;
        result.reserve(base.size() + extras.size());
        QSet<T> emitted;
        const auto emitFollowers = [&](const T& root)
        {
            QList<std::pair<T, qsizetype>> stack;
            stack.append({root, 0});
            while (!stack.isEmpty())
            {
                const T node = stack.last().first;
                const qsizetype index = stack.last().second;
                const auto it = followers.constFind(node);
                if (it == followers.cend() || index >= it->size())
                {
                    stack.removeLast();
                    continue;
                }
                ++stack.last().second;
                const T child = it->at(index);
                if (emitted.contains(child) || kept.contains(child))
                {
                    continue;
                }
                emitted.insert(child);
                result.append(child);
                stack.append({child, 0});
            }
        };
        emitFollowers(start);
        for (const T& item : base)
        {
            if (emitted.contains(item))
            {
                continue;
            }
            emitted.insert(item);
            result.append(item);
            emitFollowers(item);
        }
        for (const T& item : extras)
        {
            if (!emitted.contains(item))
            {
                emitted.insert(item);
                result.append(item);
            }
        }
        return result;
    }
}

void SyncEngine::RunLibraryPass()
{
    if (!bStarted)
    {
        return;
    }
    IndexFiles();
    BridgeOnline();
    BridgeMeta();
    BridgePlaylists();
}

void SyncEngine::RunPlaylistPass()
{
    if (!bStarted)
    {
        return;
    }
    BridgePlaylists();
    BridgeOnline();
}

void SyncEngine::FlushLocal(const QString& key)
{
    if (!bStarted || key.size() < 3 || key.at(1) != u':')
    {
        return;
    }
    const QString uid = key.mid(2);
    switch (key.at(0).unicode())
    {
    case u'o':
    {
        quint32 trackId = 0;
        const auto entry = state.entries.constFind(key);
        if (entry != state.entries.cend() && !entry->record.online.pageUrl.isEmpty())
        {
            trackId = LibrarySubsystem::Get().FindOnlineTrack(entry->record.online.pageUrl).value;
        }
        else
        {
            trackId = TrackIdForUid(uid);
        }
        BridgeOnlineKey(key, trackId);
        break;
    }
    case u'm':
    {
        const quint32 trackId = TrackIdForUid(uid);
        if (trackId != 0)
        {
            BridgeMetaKey(key, trackId);
        }
        break;
    }
    case u'p':
        BridgePlaylistKey(key, true);
        break;
    default:
        break;
    }
}

void SyncEngine::SetSeen(const QString& key, const QByteArray& seen)
{
    const auto it = state.entries.find(key);
    if (it != state.entries.end() && it->localSeen != seen)
    {
        it->localSeen = seen;
        ScheduleSave();
    }
}

void SyncEngine::EnsureUidIndex() const
{
    const LibrarySubsystem& library = LibrarySubsystem::Get();
    const quint64 revision = library.GetStructureRevision();
    if (!bUidIndexDirty && uidIndexRevision == revision)
    {
        return;
    }

    QList<const Track*> sorted;
    sorted.reserve(library.GetTracks().size());
    for (const Track& track : library.GetTracks())
    {
        sorted.append(&track);
    }
    std::sort(sorted.begin(), sorted.end(), [](const Track* a, const Track* b)
    {
        return a->id.value < b->id.value;
    });

    QHash<QString, quint32> index;
    QHash<quint32, QString> uids;
    for (const Track* track : sorted)
    {
        if (track->IsOnline() && track->stream.bSaved)
        {
            index.insert(Sync::OnlineUid(track->stream.pageUrl), track->id.value);
        }
    }
    for (const Track* track : sorted)
    {
        if (!track->IsOnline() || !track->stream.bSaved || track->file.path.isEmpty())
        {
            continue;
        }
        const auto local = state.files.constFind(Sync::PathKey(track->file.path));
        if (local != state.files.cend() && !local->uid.isEmpty() && !index.contains(local->uid))
        {
            index.insert(local->uid, track->id.value);
        }
    }
    for (const Track* track : sorted)
    {
        if (track->IsOnline() || track->file.path.isEmpty())
        {
            continue;
        }
        const auto local = state.files.constFind(Sync::PathKey(track->file.path));
        if (local == state.files.cend() || local->uid.isEmpty())
        {
            continue;
        }
        const QString uid = local->uid;
        if (!index.contains(uid))
        {
            index.insert(uid, track->id.value);
        }
        QString onlineUid;
        const auto entry = state.entries.constFind(Sync::MakeKey(Sync::Kind::File, uid));
        if (entry != state.entries.cend())
        {
            onlineUid = entry->record.file.onlineUid;
        }
        if (!onlineUid.isEmpty() && !index.contains(onlineUid))
        {
            index.insert(onlineUid, track->id.value);
        }
        uids.insert(track->id.value, onlineUid.isEmpty() ? uid : onlineUid);
    }

    QMultiHash<QString, QString> copies;
    for (auto it = state.files.cbegin(); it != state.files.cend(); ++it)
    {
        if (it->uid.isEmpty())
        {
            continue;
        }
        const auto entry = state.entries.constFind(Sync::MakeKey(Sync::Kind::File, it->uid));
        if (entry != state.entries.cend() && !entry->record.bDeleted && !entry->record.file.onlineUid.isEmpty())
        {
            copies.insert(entry->record.file.onlineUid, it.key());
        }
    }
    copiesByOnline = std::move(copies);

    if (index != uidIndex || uids != trackUids)
    {
        uidIndex = std::move(index);
        trackUids = std::move(uids);
        ++uidIndexGeneration;
    }
    uidIndexRevision = revision;
    bUidIndexDirty = false;
}

QString SyncEngine::TrackUid(const Track& track) const
{
    if (track.IsOnline())
    {
        return Sync::OnlineUid(track.stream.pageUrl);
    }
    if (track.file.path.isEmpty())
    {
        return {};
    }
    EnsureUidIndex();
    return trackUids.value(track.id.value);
}

quint32 SyncEngine::TrackIdForUid(const QString& uid) const
{
    if (uid.isEmpty())
    {
        return 0;
    }
    EnsureUidIndex();
    return uidIndex.value(uid, 0);
}

void SyncEngine::BridgeOnline()
{
    const LibrarySubsystem& library = LibrarySubsystem::Get();
    QHash<QString, quint32> keys;
    bool bAnySaved = false;
    for (const Track& track : library.GetTracks())
    {
        if (!track.IsOnline())
        {
            continue;
        }
        const QString key = Sync::MakeKey(Sync::Kind::Online, Sync::OnlineUid(track.stream.pageUrl));
        if (track.stream.bSaved)
        {
            bAnySaved = true;
            keys.insert(key, track.id.value);
        }
        else if (state.entries.contains(key))
        {
            keys.insert(key, track.id.value);
        }
    }
    for (auto it = state.entries.cbegin(); it != state.entries.cend(); ++it)
    {
        if (it->record.kind == Sync::Kind::Online && !keys.contains(it.key()))
        {
            keys.insert(it.key(), 0);
        }
    }

    const QByteArray deadDigest = OnlineDigest(nullptr);
    QStringList vanished;
    for (auto it = keys.cbegin(); it != keys.cend(); ++it)
    {
        const auto entry = state.entries.constFind(it.key());
        if (entry == state.entries.cend() || entry->record.bDeleted || entry->localSeen.isEmpty() ||
            entry->localSeen == deadDigest)
        {
            continue;
        }
        const Track* track = it.value() != 0 ? library.FindTrack(TrackId{it.value()}) : nullptr;
        if (!track || !track->stream.bSaved)
        {
            vanished.append(it.key());
        }
    }
    if (vanished.size() >= MassVanishThreshold && !bAnySaved)
    {
        for (const QString& key : vanished)
        {
            SetSeen(key, {});
        }
    }

    for (auto it = keys.cbegin(); it != keys.cend(); ++it)
    {
        BridgeOnlineKey(it.key(), it.value());
    }
}

void SyncEngine::BridgeOnlineKey(const QString& key, quint32 trackId)
{
    const QString uid = key.mid(2);
    const Track* track =
        trackId != 0 ? OnlineTrackOfKey(LibrarySubsystem::Get().FindTrack(TrackId{trackId}), key) : nullptr;
    const bool bLocalAlive = track && track->stream.bSaved;
    const QByteArray digest = OnlineDigest(track);

    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend())
    {
        if (bLocalAlive)
        {
            Commit(key, OnlineRecordOf(uid, *track, InfoOf(trackId)));
            SetSeen(key, digest);
        }
        return;
    }
    const Sync::Record record = entry->record;
    const QByteArray seen = entry->localSeen;
    const bool bRecordAlive = !record.bDeleted;

    if (seen == digest)
    {
        if (bLocalAlive != bRecordAlive)
        {
            ApplyOnline(key);
        }
        return;
    }

    if (seen.isEmpty())
    {
        if (bLocalAlive && bRecordAlive)
        {
            Sync::Record filled = record;
            if (FillOnline(filled, *track, InfoOf(trackId)))
            {
                Commit(key, filled);
            }
            ApplyOnline(key);
        }
        else if (bRecordAlive)
        {
            ApplyOnline(key);
        }
        else if (bLocalAlive)
        {
            Commit(key, OnlineRecordOf(uid, *track, InfoOf(trackId)));
            SetSeen(key, digest);
        }
        else
        {
            SetSeen(key, digest);
        }
        return;
    }

    if (bLocalAlive)
    {
        if (!bRecordAlive)
        {
            Commit(key, OnlineRecordOf(uid, *track, InfoOf(trackId)));
        }
        else
        {
            Sync::Record filled = record;
            if (FillOnline(filled, *track, InfoOf(trackId)))
            {
                Commit(key, filled);
            }
        }
    }
    else if (bRecordAlive)
    {
        Sync::Record tombstone = record;
        tombstone.bDeleted = true;
        Commit(key, tombstone);
    }
    SetSeen(key, digest);
}

void SyncEngine::ApplyOnline(const QString& key)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend())
    {
        return;
    }
    const Sync::Record record = entry->record;
    const QString uid = key.mid(2);
    const bool bUrlMatches = !record.online.pageUrl.isEmpty() && Sync::OnlineUid(record.online.pageUrl) == uid;
    LibrarySubsystem& library = LibrarySubsystem::Get();
    const auto findOnline = [&library, &key](TrackId id) -> const Track*
    {
        return id.IsValid() ? OnlineTrackOfKey(library.FindTrack(id), key) : nullptr;
    };

    TrackId id;
    if (bUrlMatches)
    {
        id = library.FindOnlineTrack(record.online.pageUrl);
    }
    if (!id.IsValid())
    {
        const TrackId indexed{TrackIdForUid(uid)};
        if (findOnline(indexed))
        {
            id = indexed;
        }
    }
    const Track* track = findOnline(id);

    if (!record.bDeleted)
    {
        const auto hasSeparateCopy = [&]()
        {
            EnsureUidIndex();
            for (auto copy = copiesByOnline.constFind(uid); copy != copiesByOnline.cend() && copy.key() == uid; ++copy)
            {
                const auto local = state.files.constFind(copy.value());
                if (local == state.files.cend())
                {
                    continue;
                }
                const TrackId owner = library.FindTrackByPath(local->path);
                const Track* ownerTrack = owner.IsValid() ? library.FindTrack(owner) : nullptr;
                if (ownerTrack && !ownerTrack->IsOnline())
                {
                    return true;
                }
            }
            return false;
        };

        if ((!track || track->IsTransient()) && bUrlMatches && !hasSeparateCopy())
        {
            const bool bIndexClean = !bUidIndexDirty && uidIndexRevision == library.GetStructureRevision();
            TagsComponent tags;
            if (record.online.info.title.isEmpty() && track)
            {
                tags = track->tags;
            }
            else
            {
                tags.title = record.online.info.title;
                tags.artist = record.online.info.artist;
                tags.album = record.online.info.album;
                tags.durationMs = record.online.info.durationMs;
            }
            id = library.AddOnlineTrack(record.online.pageUrl, tags, record.online.thumbnailUrl,
                                        record.online.channelUrl);
            library.SetSaved(id, true);
            if (bIndexClean)
            {
                uidIndex.insert(uid, id.value);
                ++uidIndexGeneration;
                uidIndexRevision = library.GetStructureRevision();
            }
            else
            {
                bUidIndexDirty = true;
            }
        }
        else if (track && track->stream.bSaved && track->stream.channelUrl.isEmpty() &&
                 !record.online.channelUrl.isEmpty())
        {
            const QString pageUrl = track->stream.pageUrl;
            const TagsComponent tags = track->tags;
            const QString thumbnailUrl = track->stream.thumbnailUrl;
            library.AddOnlineTrack(pageUrl, tags, thumbnailUrl, record.online.channelUrl);
        }

        track = findOnline(id);
        if (track && track->stream.bSaved && !track->HasLocalFile())
        {
            EnsureUidIndex();
            QStringList copies = copiesByOnline.values(uid);
            std::sort(copies.begin(), copies.end());
            for (const QString& pathKey : copies)
            {
                const auto local = state.files.constFind(pathKey);
                if (local == state.files.cend())
                {
                    continue;
                }
                const QString path = local->path;
                const TrackId owner = library.FindTrackByPath(path);
                if (!owner.IsValid() || owner == id)
                {
                    library.SetLocalFile(id, path);
                    bUidIndexDirty = true;
                    break;
                }
            }
        }
    }
    else if (track && track->stream.bSaved && !track->HasLocalFile() && !InStaticPlaylist(id))
    {
        library.SetSaved(id, false);
        bUidIndexDirty = true;
    }

    SetSeen(key, OnlineDigest(findOnline(id)));
}

void SyncEngine::BridgeMeta()
{
    EnsureUidIndex();
    QList<std::pair<QString, quint32>> work;
    for (const Track& track : LibrarySubsystem::Get().GetTracks())
    {
        if (track.IsTransient())
        {
            continue;
        }
        const QString uid = TrackUid(track);
        if (uid.isEmpty() || uidIndex.value(uid, 0) != track.id.value)
        {
            continue;
        }
        const QString key = Sync::MakeKey(Sync::Kind::Meta, uid);
        if (track.user.labels.isEmpty() && track.user.tagEdits.isEmpty() && !state.entries.contains(key))
        {
            continue;
        }
        work.append({key, track.id.value});
    }
    for (const auto& [key, trackId] : work)
    {
        BridgeMetaKey(key, trackId);
    }
}

void SyncEngine::BridgeMetaKey(const QString& key, quint32 trackId)
{
    const Track* track = trackId != 0 ? LibrarySubsystem::Get().FindTrack(TrackId{trackId}) : nullptr;
    if (!track)
    {
        return;
    }
    const QString uid = key.mid(2);
    const QByteArray seenNow = MetaSeen(*track);
    const QStringList labels = track->user.labels;
    const QMap<QString, QString> tagEdits = track->user.tagEdits;
    const bool bLocalEmpty = labels.isEmpty() && tagEdits.isEmpty();

    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend())
    {
        if (!bLocalEmpty)
        {
            Sync::Record record;
            record.kind = Sync::Kind::Meta;
            record.uid = uid;
            record.meta.labels = labels;
            record.meta.tagEdits = tagEdits;
            Commit(key, record);
            SetSeen(key, seenNow);
        }
        return;
    }
    const Sync::Record record = entry->record;
    const QByteArray seen = entry->localSeen;

    if (seen.size() == MetaIdentitySize + DigestSize && seen.left(MetaIdentitySize) == seenNow.left(MetaIdentitySize))
    {
        if (seen == seenNow)
        {
            return;
        }
        if (!record.bDeleted && MetaDigest(record.meta.labels, record.meta.tagEdits) == seenNow.mid(MetaIdentitySize))
        {
            SetSeen(key, seenNow);
            return;
        }
        Sync::Record next = record;
        next.bDeleted = false;
        next.meta.labels = labels;
        next.meta.tagEdits = tagEdits;
        Commit(key, next);
        SetSeen(key, seenNow);
        return;
    }

    if (bLocalEmpty)
    {
        ApplyMeta(key);
        return;
    }
    if (record.bDeleted || (record.meta.labels.isEmpty() && record.meta.tagEdits.isEmpty()))
    {
        Sync::Record next = record;
        next.bDeleted = false;
        next.meta.labels = labels;
        next.meta.tagEdits = tagEdits;
        Commit(key, next);
        SetSeen(key, seenNow);
        return;
    }

    Sync::MetaBody merged = record.meta;
    for (const QString& label : labels)
    {
        if (!merged.labels.contains(label, Qt::CaseInsensitive))
        {
            merged.labels.append(label);
        }
    }
    for (auto it = tagEdits.cbegin(); it != tagEdits.cend(); ++it)
    {
        merged.tagEdits.insert(it.key(), it.value());
    }
    if (merged.labels != record.meta.labels || merged.tagEdits != record.meta.tagEdits)
    {
        Sync::Record next = record;
        next.meta = merged;
        Commit(key, next);
    }
    ApplyMeta(key);
}

void SyncEngine::ApplyMeta(const QString& key)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend())
    {
        return;
    }
    const quint32 trackId = TrackIdForUid(key.mid(2));
    if (trackId == 0)
    {
        return;
    }
    const Sync::Record record = entry->record;
    LibrarySubsystem& library = LibrarySubsystem::Get();
    const Track* track = library.FindTrack(TrackId{trackId});
    if (!track)
    {
        return;
    }
    const QStringList labels = record.bDeleted ? QStringList() : record.meta.labels;
    const QMap<QString, QString> tagEdits = record.bDeleted ? QMap<QString, QString>() : record.meta.tagEdits;
    if (track->user.labels != labels || track->user.tagEdits != tagEdits)
    {
        library.ReplaceUserData(TrackId{trackId}, labels, tagEdits);
        track = library.FindTrack(TrackId{trackId});
    }
    if (track)
    {
        SetSeen(key, MetaSeen(*track));
    }
}

void SyncEngine::BridgePlaylists()
{
    EnsureUidIndex();
    const quint64 generation = uidIndexGeneration;
    const QList<Playlist>& playlists = PlaylistSubsystem::Get().GetPlaylists();
    const bool bNoPlaylists = playlists.isEmpty();
    QStringList keys;
    QSet<QString> local;
    for (const Playlist& playlist : playlists)
    {
        if (!Sync::IsUid(playlist.id))
        {
            continue;
        }
        const QString key = Sync::MakeKey(Sync::Kind::Playlist, playlist.id);
        keys.append(key);
        local.insert(key);
    }

    QStringList vanished;
    for (auto it = state.entries.cbegin(); it != state.entries.cend(); ++it)
    {
        if (it->record.kind != Sync::Kind::Playlist || local.contains(it.key()))
        {
            continue;
        }
        keys.append(it.key());
        if (!it->record.bDeleted && it->localSeen.size() >= DigestSize)
        {
            vanished.append(it.key());
        }
    }
    if (vanished.size() >= PlaylistVanishThreshold && bNoPlaylists)
    {
        for (const QString& key : vanished)
        {
            SetSeen(key, {});
        }
    }

    for (const QString& key : keys)
    {
        BridgePlaylistKey(key, false);
    }
    playlistsGeneration = generation;
}

void SyncEngine::BridgePlaylistKey(const QString& key, bool bFlush)
{
    const QString uid = key.mid(2);
    PlaylistSubsystem& playlists = PlaylistSubsystem::Get();
    const LibrarySubsystem& library = LibrarySubsystem::Get();
    const auto entry = state.entries.constFind(key);
    const bool bHasEntry = entry != state.entries.cend();
    const Sync::Record stored = bHasEntry ? entry->record : Sync::Record();
    QByteArray seenDigest;
    MemberUids known;
    const bool bSeenValid = bHasEntry && ParseSeen(entry->localSeen, seenDigest, known);

    const Playlist* found = playlists.Find(uid);
    if (!found)
    {
        if (!bHasEntry)
        {
            return;
        }
        if (stored.bDeleted)
        {
            SetSeen(key, {});
            return;
        }
        if (bSeenValid)
        {
            Sync::Record tombstone = stored;
            tombstone.bDeleted = true;
            Commit(key, tombstone);
            SetSeen(key, {});
            return;
        }
        if (!bFlush && !HasConflict(key))
        {
            ApplyPlaylist(key, true);
        }
        return;
    }

    Playlist playlist = *found;
    const QByteArray digest = PlaylistDigest(playlist);
    bool bGained = false;
    for (auto member = known.cbegin(); member != known.cend() && !bGained; ++member)
    {
        if (member.value().isEmpty())
        {
            const Track* track = library.FindTrack(TrackId{member.key()});
            bGained = track && !TrackUid(*track).isEmpty();
        }
    }

    if (bSeenValid && seenDigest == digest && !bGained)
    {
        if (bFlush || HasConflict(key))
        {
            return;
        }
        if (stored.bDeleted)
        {
            playlists.Delete(uid);
            SetSeen(key, {});
            return;
        }
        if (playlistsGeneration != uidIndexGeneration)
        {
            MemberUids members;
            if (ApplyBodyTo(stored, known, playlist, members))
            {
                playlists.Upsert(playlist);
            }
            SetSeen(key, PackSeen(PlaylistDigest(playlist), members));
        }
        return;
    }

    if (!bFlush && !IsIndexIdle())
    {
        const QList<TrackId>& localMembers = playlist.IsSmart() ? playlist.excluded : playlist.tracks;
        const bool bPending = std::any_of(localMembers.cbegin(), localMembers.cend(), [&](const TrackId id)
        {
            if (!known.value(id.value).isEmpty())
            {
                return false;
            }
            const Track* track = library.FindTrack(id);
            return track && !track->IsOnline() && track->HasLocalFile() && TrackUid(*track).isEmpty();
        });
        if (bPending)
        {
            return;
        }
    }

    const Sync::Conflict* conflict = FindConflict(key);
    const bool bPlaylistConflict = conflict && conflict->type == Sync::ConflictType::Playlist;
    const Sync::Record mine = bPlaylistConflict ? conflict->mine : Sync::Record();
    const Sync::PlaylistBody* previous = nullptr;
    if (bPlaylistConflict && !mine.bDeleted)
    {
        previous = &mine.playlist;
    }
    else if (bHasEntry && !stored.bDeleted)
    {
        previous = &stored.playlist;
    }

    MemberUids members;
    const Sync::PlaylistBody body = BodyOf(playlist, previous, known, members);
    const bool bHasReference = bPlaylistConflict || bHasEntry;
    const Sync::Record& reference = bPlaylistConflict ? mine : stored;
    const bool bSame = bHasReference && !reference.bDeleted && SamePlaylist(reference.playlist, body) &&
                       reference.playlist.createdMs == body.createdMs;
    if (!bSame)
    {
        Sync::Record record = bHasReference ? reference : Sync::Record();
        record.kind = Sync::Kind::Playlist;
        record.uid = uid;
        record.bDeleted = false;
        record.playlist = body;
        Commit(key, record);
    }

    const auto current = state.entries.constFind(key);
    if (current != state.entries.cend() && !current->record.bDeleted && !HasConflict(key))
    {
        const Sync::Record record = current->record;
        Playlist applied = playlist;
        MemberUids appliedMembers;
        if (ApplyBodyTo(record, members, applied, appliedMembers))
        {
            playlists.Upsert(applied);
        }
        SetSeen(key, PackSeen(PlaylistDigest(applied), appliedMembers));
    }
    else
    {
        SetSeen(key, PackSeen(digest, members));
    }
}

void SyncEngine::ApplyPlaylist(const QString& key, bool bForce)
{
    const auto entry = state.entries.constFind(key);
    if (entry == state.entries.cend())
    {
        return;
    }
    if (!bForce && HasConflict(key))
    {
        return;
    }
    const QString uid = key.mid(2);
    const Sync::Record record = entry->record;
    QByteArray seenDigest;
    MemberUids known;
    const bool bSeenValid = ParseSeen(entry->localSeen, seenDigest, known);
    PlaylistSubsystem& playlists = PlaylistSubsystem::Get();
    const Playlist* found = playlists.Find(uid);

    if (!bForce)
    {
        if (found && (!bSeenValid || PlaylistDigest(*found) != seenDigest))
        {
            return;
        }
        if (!found && bSeenValid)
        {
            return;
        }
    }

    if (record.bDeleted)
    {
        if (found)
        {
            playlists.Delete(uid);
        }
        SetSeen(key, {});
        return;
    }

    const bool bExisted = found != nullptr;
    Playlist playlist = bExisted ? *found : Playlist();
    if (!bExisted)
    {
        playlist.id = uid;
    }
    MemberUids members;
    const bool bChanged = ApplyBodyTo(record, known, playlist, members);
    if (!bExisted || bChanged)
    {
        playlists.Upsert(playlist);
    }
    SetSeen(key, PackSeen(PlaylistDigest(playlist), members));
}

Sync::PlaylistBody SyncEngine::BodyOf(const Playlist& playlist, const Sync::PlaylistBody* previous,
                                      const MemberUids& known, MemberUids& members) const
{
    Sync::PlaylistBody body;
    body.name = playlist.name;
    body.bSmart = playlist.IsSmart();
    body.createdMs = playlist.created.isValid() ? playlist.created.toMSecsSinceEpoch() : 0;
    const QList<TrackId>& localMembers = body.bSmart ? playlist.excluded : playlist.tracks;

    QSet<QString> snapshot;
    for (const QString& uid : known)
    {
        if (!uid.isEmpty())
        {
            snapshot.insert(uid);
        }
    }
    const bool bSameKind = previous && previous->bSmart == body.bSmart;
    QStringList previousList;
    if (bSameKind)
    {
        if (body.bSmart)
        {
            const std::optional<SmartDefinition> definition = ParseSmart(previous->smart);
            if (definition)
            {
                previousList = definition->excluded;
            }
        }
        else
        {
            previousList = previous->tracks;
        }
    }
    const QSet<QString> previousSet(previousList.cbegin(), previousList.cend());

    const LibrarySubsystem& library = LibrarySubsystem::Get();
    QStringList uids;
    QSet<QString> present;
    for (const TrackId id : localMembers)
    {
        QString uid = known.value(id.value);
        if (uid.isEmpty())
        {
            if (const Track* track = library.FindTrack(id))
            {
                uid = TrackUid(*track);
            }
        }
        members.insert(id.value, uid);
        if (uid.isEmpty() || present.contains(uid))
        {
            continue;
        }
        if (bSameKind && snapshot.contains(uid) && !previousSet.contains(uid))
        {
            continue;
        }
        present.insert(uid);
        uids.append(uid);
    }

    QStringList extras;
    for (const QString& uid : previousList)
    {
        if (!present.contains(uid) && !snapshot.contains(uid))
        {
            extras.append(uid);
        }
    }
    uids = Interleave(uids, previousList, extras, QString());

    if (previous)
    {
        body.extras = previous->extras;
    }
    if (body.bSmart)
    {
        body.smart = BuildSmart(playlist, uids);
    }
    else
    {
        body.tracks = uids;
    }
    return body;
}

bool SyncEngine::ApplyBodyTo(const Sync::Record& record, const MemberUids& known, Playlist& playlist,
                             MemberUids& members) const
{
    const QByteArray before = PlaylistDigest(playlist);
    const Sync::PlaylistBody& body = record.playlist;
    const LibrarySubsystem& library = LibrarySubsystem::Get();
    const bool bWasSmart = playlist.IsSmart();
    const QList<TrackId> localList = bWasSmart ? playlist.excluded : playlist.tracks;
    const auto identityOf = [&](TrackId id)
    {
        QString uid = known.value(id.value);
        if (uid.isEmpty())
        {
            if (const Track* track = library.FindTrack(id))
            {
                uid = TrackUid(*track);
            }
        }
        return uid;
    };
    members.clear();

    playlist.id = record.uid;
    if (!body.name.isEmpty())
    {
        playlist.name = body.name;
    }
    else if (playlist.name.isEmpty())
    {
        playlist.name = QStringLiteral("Playlist");
    }
    playlist.kind = body.bSmart ? Playlist::Kind::Smart : Playlist::Kind::Static;
    if (body.createdMs > 0)
    {
        playlist.created = QDateTime::fromMSecsSinceEpoch(body.createdMs);
    }
    else if (!playlist.created.isValid())
    {
        playlist.created = QDateTime::currentDateTime();
    }

    std::optional<SmartDefinition> definition;
    if (body.bSmart)
    {
        definition = ParseSmart(body.smart);
        if (!definition)
        {
            if (!bWasSmart)
            {
                playlist.rules.clear();
                playlist.bMatchAll = true;
                playlist.sort = QStringLiteral("added");
                playlist.limit = 0;
                playlist.excluded.clear();
            }
            for (const TrackId id : playlist.excluded)
            {
                members.insert(id.value, identityOf(id));
            }
            playlist.tracks.clear();
            return PlaylistDigest(playlist) != before;
        }
    }

    QHash<QString, quint32> bySnapshot;
    for (auto it = known.cbegin(); it != known.cend(); ++it)
    {
        if (it.value().isEmpty())
        {
            continue;
        }
        const auto existing = bySnapshot.constFind(it.value());
        if (existing == bySnapshot.cend() || it.key() < existing.value())
        {
            bySnapshot.insert(it.value(), it.key());
        }
    }

    const QStringList& uids = body.bSmart ? definition->excluded : body.tracks;
    QList<TrackId> resolved;
    QSet<quint32> used;
    for (const QString& uid : uids)
    {
        quint32 id = 0;
        const auto snapshot = bySnapshot.constFind(uid);
        if (snapshot != bySnapshot.cend() && library.FindTrack(TrackId{snapshot.value()}))
        {
            id = snapshot.value();
        }
        else
        {
            id = TrackIdForUid(uid);
        }
        if (id == 0 || used.contains(id))
        {
            continue;
        }
        used.insert(id);
        resolved.append(TrackId{id});
        members.insert(id, uid);
    }

    QList<TrackId> keep;
    if (bWasSmart == body.bSmart)
    {
        for (const TrackId id : localList)
        {
            if (used.contains(id.value) || !identityOf(id).isEmpty())
            {
                continue;
            }
            used.insert(id.value);
            keep.append(id);
            members.insert(id.value, QString());
        }
    }
    resolved = Interleave(resolved, localList, keep, TrackId{});

    if (body.bSmart)
    {
        playlist.rules = definition->rules;
        playlist.bMatchAll = definition->bMatchAll;
        playlist.sort = definition->sort;
        playlist.limit = definition->limit;
        playlist.excluded = resolved;
        playlist.tracks.clear();
    }
    else
    {
        playlist.tracks = resolved;
        playlist.rules.clear();
        playlist.bMatchAll = true;
        playlist.sort = QStringLiteral("added");
        playlist.limit = 0;
        playlist.excluded.clear();
    }
    return PlaylistDigest(playlist) != before;
}
