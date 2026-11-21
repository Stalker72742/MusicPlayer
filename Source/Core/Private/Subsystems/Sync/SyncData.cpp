//
// Created by Stalker7274 on 19.11.2026.
//

#include "SyncData.h"

#include "sync.qpb.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace proto = soundlink::sync::v1;

namespace Sync
{
    namespace
    {
        constexpr quint32 StateMagic = 0x534C5354;
        constexpr quint32 StateVersion = 1;
        constexpr int MaxHistory = 16;
        constexpr qsizetype MaxText = 4096;
        constexpr quint64 MaxFileSize = quint64(1) << 40;

        proto::TrackInfo InfoToProto(const TrackInfo& info)
        {
            proto::TrackInfo message;
            message.setTitle(info.title);
            message.setArtist(info.artist);
            message.setAlbum(info.album);
            message.setDurationMs(info.durationMs);
            return message;
        }

        TrackInfo InfoFromProto(const proto::TrackInfo& message)
        {
            TrackInfo info;
            info.title = message.title().left(MaxText);
            info.artist = message.artist().left(MaxText);
            info.album = message.album().left(MaxText);
            info.durationMs = std::max<qint64>(0, message.durationMs());
            return info;
        }

        QHash<QString, QString> ToHash(const QMap<QString, QString>& map)
        {
            QHash<QString, QString> hash;
            hash.reserve(map.size());
            for (auto it = map.cbegin(); it != map.cend(); ++it)
            {
                hash.insert(it.key(), it.value());
            }
            return hash;
        }

        QMap<QString, QString> ToMap(const QHash<QString, QString>& hash)
        {
            QMap<QString, QString> map;
            for (auto it = hash.cbegin(); it != hash.cend(); ++it)
            {
                map.insert(it.key(), it.value());
            }
            return map;
        }
    }

    static QDataStream& operator<<(QDataStream& out, const TrackInfo& value)
    {
        return out << value.title << value.artist << value.album << value.durationMs;
    }

    static QDataStream& operator>>(QDataStream& in, TrackInfo& value)
    {
        return in >> value.title >> value.artist >> value.album >> value.durationMs;
    }

    static QDataStream& operator<<(QDataStream& out, const FileBody& value)
    {
        return out << value.sha256 << value.size << value.relPath << value.mtimeMs << value.history << value.info << value.onlineUid;
    }

    static QDataStream& operator>>(QDataStream& in, FileBody& value)
    {
        return in >> value.sha256 >> value.size >> value.relPath >> value.mtimeMs >> value.history >> value.info >> value.onlineUid;
    }

    static QDataStream& operator<<(QDataStream& out, const OnlineBody& value)
    {
        return out << value.pageUrl << value.info << value.thumbnailUrl << value.channelUrl << value.bSaved;
    }

    static QDataStream& operator>>(QDataStream& in, OnlineBody& value)
    {
        return in >> value.pageUrl >> value.info >> value.thumbnailUrl >> value.channelUrl >> value.bSaved;
    }

    static QDataStream& operator<<(QDataStream& out, const MetaBody& value)
    {
        return out << value.labels << value.tagEdits;
    }

    static QDataStream& operator>>(QDataStream& in, MetaBody& value)
    {
        return in >> value.labels >> value.tagEdits;
    }

    static QDataStream& operator<<(QDataStream& out, const PlaylistBody& value)
    {
        return out << value.name << value.bSmart << value.createdMs << value.tracks << value.smart << value.extras;
    }

    static QDataStream& operator>>(QDataStream& in, PlaylistBody& value)
    {
        return in >> value.name >> value.bSmart >> value.createdMs >> value.tracks >> value.smart >> value.extras;
    }

    static QDataStream& operator<<(QDataStream& out, const Record& value)
    {
        out << quint8(value.kind) << value.uid << value.vv << value.bDeleted << value.hlc << value.changedBy;
        switch (value.kind)
        {
        case Kind::File:
            out << value.file;
            break;
        case Kind::Online:
            out << value.online;
            break;
        case Kind::Meta:
            out << value.meta;
            break;
        case Kind::Playlist:
            out << value.playlist;
            break;
        }
        return out;
    }

    static QDataStream& operator>>(QDataStream& in, Record& value)
    {
        quint8 kind = 0;
        in >> kind >> value.uid >> value.vv >> value.bDeleted >> value.hlc >> value.changedBy;
        value.kind = static_cast<Kind>(kind);
        switch (value.kind)
        {
        case Kind::File:
            in >> value.file;
            break;
        case Kind::Online:
            in >> value.online;
            break;
        case Kind::Meta:
            in >> value.meta;
            break;
        case Kind::Playlist:
            in >> value.playlist;
            break;
        default:
            in.setStatus(QDataStream::ReadCorruptData);
            break;
        }
        return in;
    }

    static QDataStream& operator<<(QDataStream& out, const Entry& value)
    {
        return out << value.record << value.seq << value.haveBy << value.echoPeer << value.localSeen;
    }

    static QDataStream& operator>>(QDataStream& in, Entry& value)
    {
        return in >> value.record >> value.seq >> value.haveBy >> value.echoPeer >> value.localSeen;
    }

    static QDataStream& operator<<(QDataStream& out, const LocalFile& value)
    {
        return out << value.path << value.size << value.mtimeMs << value.sha << value.uid;
    }

    static QDataStream& operator>>(QDataStream& in, LocalFile& value)
    {
        return in >> value.path >> value.size >> value.mtimeMs >> value.sha >> value.uid;
    }

    static QDataStream& operator<<(QDataStream& out, const Peer& value)
    {
        return out << value.id << value.name << qint32(value.platform) << value.psk << value.addresses << value.lastSeenMs << value.peerEpoch << value.appliedSeq;
    }

    static QDataStream& operator>>(QDataStream& in, Peer& value)
    {
        qint32 platform = 0;
        in >> value.id >> value.name >> platform >> value.psk >> value.addresses >> value.lastSeenMs >> value.peerEpoch >> value.appliedSeq;
        value.platform = platform;
        return in;
    }

    static QDataStream& operator<<(QDataStream& out, const Conflict& value)
    {
        return out << value.id << quint8(value.type) << value.key << value.otherKey << value.peerId << value.peerName << value.mine << value.theirs << value.createdMs << value.localPath;
    }

    static QDataStream& operator>>(QDataStream& in, Conflict& value)
    {
        quint8 type = 0;
        in >> value.id >> type >> value.key >> value.otherKey >> value.peerId >> value.peerName >> value.mine >> value.theirs >> value.createdMs >> value.localPath;
        value.type = static_cast<ConflictType>(type);
        if (type < quint8(ConflictType::Content) || type > quint8(ConflictType::LocalDelete))
        {
            in.setStatus(QDataStream::ReadCorruptData);
        }
        return in;
    }

    Order Compare(const VersionVector& a, const VersionVector& b)
    {
        bool bGreater = false;
        bool bLess = false;
        for (auto it = a.cbegin(); it != a.cend(); ++it)
        {
            const quint64 other = b.value(it.key(), 0);
            if (it.value() > other)
            {
                bGreater = true;
            }
            else if (it.value() < other)
            {
                bLess = true;
            }
        }
        for (auto it = b.cbegin(); it != b.cend(); ++it)
        {
            if (it.value() > 0 && !a.contains(it.key()))
            {
                bLess = true;
            }
        }
        if (bGreater && bLess)
        {
            return Order::Concurrent;
        }
        if (bGreater)
        {
            return Order::Newer;
        }
        return bLess ? Order::Older : Order::Equal;
    }

    VersionVector Merged(const VersionVector& a, const VersionVector& b)
    {
        VersionVector result = a;
        for (auto it = b.cbegin(); it != b.cend(); ++it)
        {
            quint64& value = result[it.key()];
            value = std::max(value, it.value());
        }
        return result;
    }

    QString MakeKey(Kind kind, const QString& uid)
    {
        switch (kind)
        {
        case Kind::File:
            return QStringLiteral("f:") + uid;
        case Kind::Online:
            return QStringLiteral("o:") + uid;
        case Kind::Meta:
            return QStringLiteral("m:") + uid;
        case Kind::Playlist:
            return QStringLiteral("p:") + uid;
        }
        return QStringLiteral("?:") + uid;
    }

    QString Record::Key() const
    {
        return MakeKey(kind, uid);
    }

    bool LoadState(const QString& path, State& state)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }
        QDataStream in(&file);
        in.setVersion(QDataStream::Qt_6_8);
        quint32 magic = 0;
        quint32 version = 0;
        in >> magic >> version;
        if (magic != StateMagic || version != StateVersion)
        {
            return false;
        }

        State loaded;
        in >> loaded.deviceId >> loaded.epoch >> loaded.seq >> loaded.hlc;
        quint32 count = 0;
        in >> count;
        for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
        {
            Entry entry;
            in >> entry;
            loaded.entries.insert(entry.record.Key(), entry);
        }
        in >> count;
        for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
        {
            LocalFile local;
            in >> local;
            loaded.files.insert(PathKey(local.path), local);
        }
        in >> count;
        for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
        {
            Peer peer;
            in >> peer;
            loaded.peers.insert(peer.id, peer);
        }
        in >> count;
        for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
        {
            Conflict conflict;
            in >> conflict;
            loaded.conflicts.append(conflict);
        }
        in >> loaded.keptNamePairs >> loaded.pendingNameChecks >> loaded.placements >> loaded.parked;
        if (in.status() != QDataStream::Ok || !IsDeviceId(loaded.deviceId) || loaded.epoch == 0)
        {
            return false;
        }
        state = std::move(loaded);
        return true;
    }

    bool SaveState(const QString& path, const State& state)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            return false;
        }
        QDataStream out(&file);
        out.setVersion(QDataStream::Qt_6_8);
        out << StateMagic << StateVersion;
        out << state.deviceId << state.epoch << state.seq << state.hlc;
        out << quint32(state.entries.size());
        for (const Entry& entry : state.entries)
        {
            out << entry;
        }
        out << quint32(state.files.size());
        for (const LocalFile& local : state.files)
        {
            out << local;
        }
        out << quint32(state.peers.size());
        for (const Peer& peer : state.peers)
        {
            out << peer;
        }
        out << quint32(state.conflicts.size());
        for (const Conflict& conflict : state.conflicts)
        {
            out << conflict;
        }
        out << state.keptNamePairs << state.pendingNameChecks << state.placements << state.parked;
        if (out.status() != QDataStream::Ok)
        {
            file.cancelWriting();
            return false;
        }
        return file.commit();
    }

    proto::SyncRecord ToProto(const Record& record)
    {
        proto::SyncRecord message;
        message.setKind(static_cast<proto::RecordKindGadget::RecordKind>(static_cast<int>(record.kind)));
        message.setUid(record.uid);
        proto::SyncRecord::VvEntry vv;
        vv.reserve(record.vv.size());
        for (auto it = record.vv.cbegin(); it != record.vv.cend(); ++it)
        {
            vv.insert(it.key(), it.value());
        }
        message.setVv(std::move(vv));
        message.setDeleted(record.bDeleted);
        message.setHlc(record.hlc);
        message.setChangedBy(record.changedBy);
        message.setHave(record.bHave);
        switch (record.kind)
        {
        case Kind::File:
        {
            proto::FileBody body;
            body.setSha256(record.file.sha256);
            body.setSize(record.file.size);
            body.setRelPath(record.file.relPath);
            body.setMtimeMs(record.file.mtimeMs);
            body.setHistory(record.file.history);
            body.setInfo(InfoToProto(record.file.info));
            body.setOnlineUid(record.file.onlineUid);
            message.setFile(std::move(body));
            break;
        }
        case Kind::Online:
        {
            proto::OnlineBody body;
            body.setPageUrl(record.online.pageUrl);
            body.setInfo(InfoToProto(record.online.info));
            body.setThumbnailUrl(record.online.thumbnailUrl);
            body.setChannelUrl(record.online.channelUrl);
            body.setSaved(record.online.bSaved);
            message.setOnline(std::move(body));
            break;
        }
        case Kind::Meta:
        {
            proto::MetaBody body;
            body.setLabels(record.meta.labels);
            body.setTagEdits(ToHash(record.meta.tagEdits));
            message.setMeta(std::move(body));
            break;
        }
        case Kind::Playlist:
        {
            proto::PlaylistBody body;
            body.setName(record.playlist.name);
            body.setKind(record.playlist.bSmart ? proto::PlaylistKindGadget::PlaylistKind::PLAYLIST_SMART
                                                : proto::PlaylistKindGadget::PlaylistKind::PLAYLIST_STATIC);
            body.setCreatedMs(record.playlist.createdMs);
            body.setTracks(record.playlist.tracks);
            body.setSmart(record.playlist.smart);
            body.setExtras(ToHash(record.playlist.extras));
            message.setPlaylist(std::move(body));
            break;
        }
        }
        return message;
    }

    std::optional<Record> FromProto(const proto::SyncRecord& message)
    {
        Record record;
        const int kind = static_cast<int>(message.kind());
        if (kind < int(Kind::File) || kind > int(Kind::Playlist) || !IsUid(message.uid()) || !IsDeviceId(message.changedBy()))
        {
            return std::nullopt;
        }
        record.kind = static_cast<Kind>(kind);
        record.uid = message.uid();
        const proto::SyncRecord::VvEntry& vv = message.vv();
        if (vv.isEmpty() || vv.size() > 1024)
        {
            return std::nullopt;
        }
        for (auto it = vv.cbegin(); it != vv.cend(); ++it)
        {
            if (!IsDeviceId(it.key()))
            {
                return std::nullopt;
            }
            if (it.value() > 0)
            {
                record.vv.insert(it.key(), it.value());
            }
        }
        if (record.vv.isEmpty())
        {
            return std::nullopt;
        }
        record.bDeleted = message.deleted();
        record.hlc = message.hlc();
        record.changedBy = message.changedBy();
        record.bHave = message.have();

        switch (record.kind)
        {
        case Kind::File:
        {
            if (!message.hasFile())
            {
                return std::nullopt;
            }
            const proto::FileBody& body = message.file();
            if (body.sha256().size() != 32 || body.size() > MaxFileSize || body.relPath().size() > 1024)
            {
                return std::nullopt;
            }
            record.file.sha256 = body.sha256();
            record.file.size = body.size();
            record.file.relPath = body.relPath();
            record.file.mtimeMs = body.mtimeMs();
            for (const QByteArray& sha : body.history())
            {
                if (sha.size() == 32 && record.file.history.size() < MaxHistory)
                {
                    record.file.history.append(sha);
                }
            }
            if (body.hasInfo())
            {
                record.file.info = InfoFromProto(body.info());
            }
            if (!body.onlineUid().isEmpty() && IsUid(body.onlineUid()))
            {
                record.file.onlineUid = body.onlineUid();
            }
            break;
        }
        case Kind::Online:
        {
            if (!message.hasOnline())
            {
                return std::nullopt;
            }
            const proto::OnlineBody& body = message.online();
            const QString& url = body.pageUrl();
            if (url.size() > 2048 || !(url.startsWith(QLatin1String("https://")) || url.startsWith(QLatin1String("http://"))))
            {
                return std::nullopt;
            }
            record.online.pageUrl = url;
            if (body.hasInfo())
            {
                record.online.info = InfoFromProto(body.info());
            }
            record.online.thumbnailUrl = body.thumbnailUrl().left(2048);
            record.online.channelUrl = body.channelUrl().left(2048);
            record.online.bSaved = body.saved();
            break;
        }
        case Kind::Meta:
        {
            if (!message.hasMeta())
            {
                return std::nullopt;
            }
            const proto::MetaBody& body = message.meta();
            if (body.labels().size() > 256 || body.tagEdits().size() > 64)
            {
                return std::nullopt;
            }
            for (const QString& label : body.labels())
            {
                record.meta.labels.append(label.left(256));
            }
            for (auto it = body.tagEdits().cbegin(); it != body.tagEdits().cend(); ++it)
            {
                record.meta.tagEdits.insert(it.key().left(64), it.value().left(MaxText));
            }
            break;
        }
        case Kind::Playlist:
        {
            if (!message.hasPlaylist())
            {
                return std::nullopt;
            }
            const proto::PlaylistBody& body = message.playlist();
            if (body.tracks().size() > 100000 || body.smart().size() > 65536 || body.extras().size() > 64)
            {
                return std::nullopt;
            }
            record.playlist.name = body.name().left(512);
            record.playlist.bSmart = body.kind() == proto::PlaylistKindGadget::PlaylistKind::PLAYLIST_SMART;
            record.playlist.createdMs = body.createdMs();
            for (const QString& uid : body.tracks())
            {
                if (IsUid(uid))
                {
                    record.playlist.tracks.append(uid);
                }
            }
            record.playlist.smart = body.smart();
            record.playlist.extras = ToMap(body.extras());
            break;
        }
        }
        return record;
    }

    bool IsDeviceId(const QString& id)
    {
        if (id.size() != 32)
        {
            return false;
        }
        for (const QChar c : id)
        {
            const char16_t u = c.unicode();
            if (!((u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'f')))
            {
                return false;
            }
        }
        return true;
    }

    bool IsUid(const QString& uid)
    {
        if (uid.isEmpty() || uid.size() > 64)
        {
            return false;
        }
        for (const QChar c : uid)
        {
            const char16_t u = c.unicode();
            const bool bOk = (u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || u == u'-' || u == u'_';
            if (!bOk)
            {
                return false;
            }
        }
        return true;
    }

    QString ToHex(const QByteArray& bytes)
    {
        return QString::fromLatin1(bytes.toHex());
    }

    QString FileUid(const QByteArray& sha256)
    {
        return ToHex(sha256.left(16));
    }

    QString OnlineUid(const QString& pageUrl)
    {
        return ToHex(QCryptographicHash::hash(pageUrl.toUtf8(), QCryptographicHash::Sha256).left(16));
    }

    QString PathKey(const QString& path)
    {
        return QDir::cleanPath(path).toLower();
    }
}
