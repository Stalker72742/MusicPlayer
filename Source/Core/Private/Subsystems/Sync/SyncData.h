//
// Created by Stalker7274 on 19.11.2026.
//

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace soundlink::sync::v1
{
    class SyncRecord;
}

/// Data model of device sync (see DESIGN.md next to this file).
namespace Sync
{
    enum class Kind : quint8
    {
        File = 1,     ///< An audio file, identified by the content it was first indexed with.
        Online = 2,   ///< A saved or playlisted online track, identified by its page URL.
        Meta = 3,     ///< User labels and tag edits of a File or Online item (same uid).
        Playlist = 4  ///< A playlist, identified by its UUID.
    };

    /// Device id -> number of changes made on that device.
    using VersionVector = QMap<QString, quint64>;

    /// How version vector `a` relates to `b`.
    enum class Order
    {
        Equal,
        Newer,      ///< a dominates b
        Older,      ///< b dominates a
        Concurrent
    };

    Order Compare(const VersionVector& a, const VersionVector& b);
    VersionVector Merged(const VersionVector& a, const VersionVector& b);

    struct TrackInfo
    {
        QString title;
        QString artist;
        QString album;
        qint64 durationMs = 0;
    };

    struct FileBody
    {
        QByteArray sha256;          ///< Current content.
        quint64 size = 0;
        QString relPath;            ///< Placement hint, relative to the deepest library root.
        qint64 mtimeMs = 0;
        QList<QByteArray> history;  ///< Earlier contents, newest first.
        TrackInfo info;
        QString onlineUid;          ///< The downloaded copy of this Online item.
    };

    struct OnlineBody
    {
        QString pageUrl;
        TrackInfo info;
        QString thumbnailUrl;
        QString channelUrl;
        bool bSaved = false;
    };

    struct MetaBody
    {
        QStringList labels;
        QMap<QString, QString> tagEdits;
    };

    struct PlaylistBody
    {
        QString name;
        bool bSmart = false;
        qint64 createdMs = 0;
        QStringList tracks;             ///< File / Online uids (static playlists).
        QString smart;                  ///< Smart definition JSON.
        QMap<QString, QString> extras;  ///< Settings of other platforms, kept as they are.
    };

    /// One replicated item as it travels between devices. Only the body of its kind is meaningful.
    struct Record
    {
        Kind kind = Kind::File;
        QString uid;
        VersionVector vv;
        bool bDeleted = false;
        quint64 hlc = 0;
        QString changedBy;
        bool bHave = false;  ///< Wire only: the sender holds this exact content.
        FileBody file;
        OnlineBody online;
        MetaBody meta;
        PlaylistBody playlist;

        QString Key() const;
    };

    QString MakeKey(Kind kind, const QString& uid);

    /// A record as this device keeps it.
    struct Entry
    {
        Record record;
        quint64 seq = 0;          ///< Local change counter of the last change, drives index deltas.
        QSet<QString> haveBy;     ///< File: devices known to hold the current content.
        QString echoPeer;         ///< Adopted exactly as this peer sent it: not sent back to it.
        QByteArray localSeen;     ///< Online / Meta / Playlist: digest of the library state the record matches.
    };

    /// One audio file under a library folder.
    struct LocalFile
    {
        QString path;
        qint64 size = 0;
        qint64 mtimeMs = 0;
        QByteArray sha;           ///< Verified content of (size, mtime); empty until hashed.
        QString uid;              ///< The File item it is a copy of; empty until linked.
        qint64 observedMs = 0;    ///< Runtime: when (size, mtime) were first seen.
        quint32 trackId = 0;      ///< Runtime: library track id.
    };

    struct Peer
    {
        QString id;
        QString name;
        int platform = 0;
        QByteArray psk;
        QStringList addresses;    ///< Endpoints "host:port" it announced.
        qint64 lastSeenMs = 0;
        quint64 peerEpoch = 0;    ///< Database epoch of the peer that appliedSeq refers to.
        quint64 appliedSeq = 0;   ///< Last seq of the peer merged here.
    };

    enum class ConflictType : quint8
    {
        Content = 1,     ///< Changed on both sides: mine / theirs / both.
        DeleteVsEdit,    ///< Deleted on one side, changed on the other: mine / theirs.
        Name,            ///< Two different files want the same place: both / mine / theirs.
        Playlist,        ///< Playlist changed on both sides: mine / theirs / both (union).
        RemoteChange,    ///< A peer replaced or deleted a file and confirmation is on: theirs (accept) / mine (reject).
        LocalDelete      ///< Many files vanished here at once: mine (delete everywhere) / theirs (restore).
    };

    enum class Choice
    {
        Mine,
        Theirs,
        Both
    };

    struct Conflict
    {
        QString id;
        ConflictType type = ConflictType::Content;
        QString key;          ///< The local item.
        QString otherKey;     ///< Name: the incoming item.
        QString peerId;
        QString peerName;
        Record mine;          ///< This side's version when it is not the stored record (a local edit).
        Record theirs;        ///< The peer's version.
        qint64 createdMs = 0;
        QString localPath;    ///< LocalDelete: where the vanished copy was; Name: the local file.
    };

    /// Everything kept in `<AppLocalData>/sync/state.bin`.
    struct State
    {
        QString deviceId;
        quint64 epoch = 0;
        quint64 seq = 0;
        quint64 hlc = 0;
        QHash<QString, Entry> entries;     ///< By Record::Key().
        QHash<QString, LocalFile> files;   ///< By lower-case path.
        QHash<QString, Peer> peers;        ///< By device id.
        QList<Conflict> conflicts;
        QSet<QString> keptNamePairs;       ///< "localKey|incomingKey" resolved as keep both.
        QSet<QString> pendingNameChecks;   ///< Incoming File items not placed until checked against local names.
        QHash<QString, QString> placements; ///< File key -> path its next download must land at (resolutions, restores).
        QSet<QString> parked;              ///< File keys whose copies left the library folders: kept, not downloaded again.
    };

    bool LoadState(const QString& path, State& state);
    bool SaveState(const QString& path, const State& state);

    soundlink::sync::v1::SyncRecord ToProto(const Record& record);
    /// Converts and validates a received record (kind, uid, version vector, body sizes);
    /// std::nullopt when it is malformed.
    std::optional<Record> FromProto(const soundlink::sync::v1::SyncRecord& message);

    /// 32 lowercase hex characters.
    bool IsDeviceId(const QString& id);
    /// 1 to 64 characters of [0-9A-Za-z_-]: an item uid as records carry it.
    bool IsUid(const QString& uid);
    QString ToHex(const QByteArray& bytes);
    /// Uid of the File item first indexed with content `sha256`.
    QString FileUid(const QByteArray& sha256);
    /// Uid of the Online item of `pageUrl`.
    QString OnlineUid(const QString& pageUrl);
    /// Lower-case path used as the key of State::files.
    QString PathKey(const QString& path);
}
