//
// Created by Stalker7274 on 19.11.2026.
//

#pragma once

#include "SyncData.h"

#include <QHash>
#include <QMultiHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <memory>

class QFile;
class SyncSession;
struct Playlist;
struct Track;

namespace soundlink::sync::v1
{
    class BlockData;
    class BlockRequest;
    class Frame;
    class Welcome;
}

/// @brief The replication engine of device sync (see DESIGN.md next to this file).
///
/// Keeps Sync::State, pushes local changes to attached peers as index deltas, merges what they send, raises
/// conflicts, indexes the audio files of the library folders, moves file contents in blocks and mirrors saved
/// online tracks, user tags and playlists into records. It knows nothing about the network: SyncSubsystem hands
/// it established sessions of paired devices.
class SyncEngine : public QObject
{
    Q_OBJECT

public:
    /// @brief A file being received.
    struct Transfer
    {
        QString key;
        QString title;
        QString peerName; ///< Empty for a copy of a local file.
        qint64 size = 0;
        qint64 done = 0;
    };

    /// @brief What the engine is busy with.
    struct Progress
    {
        int hashing = 0;          ///< Files whose content is still to be read.
        int missingFiles = 0;     ///< File items this device should hold and does not yet.
        qint64 missingBytes = 0;
        int transfers = 0;        ///< Files being received.
        qint64 transferBytes = 0;
        qint64 transferDone = 0;
        int deferred = 0;         ///< Replacements and deletions waiting for the player to leave a file.
        QString blocked;          ///< Why downloads wait ("disk_full"); empty when they do not.
    };

    explicit SyncEngine(QObject* parent = nullptr);
    ~SyncEngine() override;

    /// @brief Reads the state from `path`.
    ///
    /// A missing or unreadable file starts a new device identity; an unreadable one is kept as `<path>.corrupted`.
    void Load(const QString& path);
    /// @brief Writes the state now.
    void Save();
    /// @brief Starts following the library, playlists and the player. Indexing waits for the first library scan.
    void Start();
    /// @brief Stops transfers and hashing, detaches all sessions and saves.
    void Stop();

    const Sync::State& GetState() const { return state; }
    QString DeviceId() const { return state.deviceId; }

    /// @brief This side's Welcome for a peer.
    soundlink::sync::v1::Welcome MakeWelcome(const QString& peerId, const QString& deviceName) const;
    /// @brief Takes an established session of a paired peer; a later session of the same peer replaces the first.
    void AttachSession(SyncSession* session);
    /// @brief Whether a session with this peer is attached.
    bool IsConnected(const QString& peerId) const;

    /// @brief Adds a paired device or updates what is known about it.
    void UpsertPeer(const Sync::Peer& peer);
    /// @brief Forgets a paired device and which files it holds. Its session, if any, is the caller's to close.
    void RemovePeer(const QString& peerId);
    /// @brief Endpoints ("host:port") a peer announced.
    void SetPeerAddresses(const QString& peerId, const QStringList& addresses);

    const QList<Sync::Conflict>& GetConflicts() const { return state.conflicts; }
    /// @brief Resolves conflicts with one choice; conflicts the choice does not apply to stay open.
    void Resolve(const QStringList& ids, Sync::Choice choice);
    /// @brief Whether a peer replacing or deleting a local file waits for the user (a RemoteChange conflict).
    void SetConfirmRemote(bool bConfirm) { bConfirmRemote = bConfirm; }

    QList<Transfer> GetTransfers() const;
    Progress GetProgress() const;

    /// @brief Rescans the library folders and indexes them again.
    void Reindex();
    /// @brief Whether files are synced: there is a library folder to place them in.
    bool IsFileSyncAvailable() const { return !placementRoot.isEmpty(); }
    /// @brief The folder received files are placed in; empty without library folders.
    QString GetPlacementRoot() const { return placementRoot; }

signals:
    /// @brief Hashing, transfers or missing files changed. Coalesced.
    void progressChanged();
    void conflictsChanged();
    /// @brief New conflicts appeared.
    void conflictsAdded(int count);
    /// @brief Local files went to the Recycle Bin because a peer deleted them.
    void filesTrashed(int count, const QString& peerName);
    /// @brief Paired devices or their connection state changed.
    void peersChanged();

private:
    /// An attached session and where its index push stands.
    struct Link
    {
        QPointer<SyncSession> session;
        QString peerId;
        quint64 sentSeq = 0;                   ///< Local seq the peer has everything up to.
        QStringList cycleKeys;                 ///< Keys of the running push cycle, by seq.
        qsizetype cycleIndex = 0;
        quint64 cycleSeq = 0;                  ///< state.seq when the cycle started; sent in IndexDone.
        bool bCycleRunning = false;
        bool bCycleAgain = false;              ///< Changes came in during the cycle.
        bool bCycleWaiting = false;            ///< A retry for transport backpressure is queued.
        bool bInitialReceived = false;         ///< The peer's first IndexDone came in.
        bool bIgnoreEcho = false;              ///< The peer's database was reset: the next cycle sends what came from it too.
        quint64 peerEpoch = 0;
        QHash<quint32, QString> blockRequests; ///< Request id -> download key.
    };

    /// A file content being received from a peer or copied from a local file.
    struct Download
    {
        QString key;
        QByteArray sha;
        qint64 size = 0;
        QString peerId;                   ///< Empty: copied from a local file.
        QString partPath;
        std::unique_ptr<QFile> file;
        qint64 received = 0;              ///< Bytes written, in order.
        qint64 requested = 0;             ///< Next offset to request.
        QHash<quint32, qint64> inflight;  ///< Request id -> offset.
        qint64 lastActivityMs = 0;
        bool bBusy = false;               ///< A background copy or check runs.
        bool bReady = false;              ///< Complete and verified, waiting to be placed.
        std::shared_ptr<std::atomic_bool> cancel;
    };

    /// A library file whose content is not known for its size and modification time.
    struct Candidate
    {
        QString path;
        qint64 size = 0;
        qint64 mtimeMs = 0;
        qint64 observedMs = 0;  ///< When this size and time were first seen.
        quint32 trackId = 0;
        bool bHashing = false;
        int failures = 0;       ///< Failed reads in a row.
        qint64 retryAtMs = 0;   ///< Not read again before this time.
        std::shared_ptr<std::atomic_bool> cancel;
    };

    /// What became of a local copy that was to be replaced.
    enum class ReplaceResult
    {
        Done,
        Deferred,  ///< The player holds it.
        Skipped,   ///< It changed on disk: the change is indexed as a local edit.
        Failed
    };

    /// A local file kept open while a peer reads it.
    struct ServedFile
    {
        std::unique_ptr<QFile> file;
        QString pathKey;
        qint64 lastUsedMs = 0;
    };

    // Records and local state (SyncEngine.cpp)
    void ScheduleSave();
    void SchedulePush();
    void ScheduleProgress();
    quint64 NextHlc();
    void ObserveHlc(quint64 hlc);
    /// A new local seq for the entry: it goes to every peer, the one it came from included.
    void Touch(const QString& key);
    /// A local change: bumps this device in the version vector and stores `record`.
    void Commit(const QString& key, Sync::Record record);
    /// Stores a record without touching its version and keeps the runtime indexes current.
    Sync::Entry& PutRecord(const QString& key, const Sync::Record& record);
    void SetSelfHave(const QString& key, bool bHave);
    /// Whether a local copy of the item holds its current content; a change is a new seq.
    void RecomputeSelfHave(const QString& uid);
    /// Whether a local copy holds the current content of a File item.
    bool HoldsCurrent(const QString& key) const;
    void RefreshNeed(const QString& key);
    void RebuildIndexes();
    void PutLocal(const QString& pathKey, const Sync::LocalFile& local);
    void RemoveLocal(const QString& pathKey);
    /// PathKey of the copy that speaks for the item: the smallest one holding the current content.
    QString PrimaryCopy(const QString& uid) const;
    QString PeerName(const QString& peerId) const;
    static bool Wins(const Sync::Record& a, const Sync::Record& b);
    /// Cancels hashing, copies and checks and waits for the running ones.
    void CancelBackgroundWork();

    // Sessions and the index push
    void DetachSession(const QString& peerId, SyncSession* session);
    void StartCycle(const QString& peerId, bool bForced);
    void ContinueCycle(const QString& peerId);
    void OnFrame(const QString& peerId, const soundlink::sync::v1::Frame& frame);
    bool AllLinksCaughtUp() const;

    // Merging
    void MergeRecord(const QString& peerId, Sync::Record record);
    /// A record for a key with an open conflict; true when the conflict went away and the record merges as usual.
    bool MergeIntoConflict(Sync::Conflict& conflict, const Sync::Record& record, const QString& peerId);
    /// Whether adopting `incoming` replaces or deletes a local file that confirmation protects.
    bool NeedsConfirmation(const Sync::Record& current, const Sync::Record& incoming) const;
    void Adopt(const QString& key, const Sync::Record& record, const QString& peerId);
    void MergeConcurrent(const QString& key, const Sync::Record& record, const QString& peerId);
    /// Brings files and the library in line with a record that changed.
    void AfterChange(const QString& key, const Sync::Record& before, const QString& peerId);
    void AfterBatch();

    // Conflicts
    Sync::Conflict* FindConflict(const QString& key);
    bool HasConflict(const QString& key) const;
    void AddConflict(Sync::ConflictType type, const QString& key, const QString& otherKey, const QString& peerId,
                     const Sync::Record& mine, const Sync::Record& theirs, const QString& localPath = {});
    bool ResolveOne(const Sync::Conflict& conflict, Sync::Choice choice);
    /// Deletes an item as a local decision and applies it to the files.
    void TombstoneItem(const QString& key);
    void RemoveConflict(const QString& id);
    /// After state.conflicts changed: keeps conflictedKeys current and coalesces conflictsChanged().
    void ConflictsModified();
    void RunNameChecks();
    void RecheckNameConflicts();
    void FlushNotifications();

    // Files: index (SyncEngineFiles.cpp)
    QStringList LibraryRoots() const;
    void UpdatePlacementRoot(const QStringList& roots);
    QString RelPathFor(const QString& path) const;
    QString PlacementPathFor(const QString& relPath) const;
    void IndexFiles();
    bool IsIndexIdle() const;
    void ScheduleHashing();
    void OnHashed(const QString& pathKey, const Candidate& candidate, const QByteArray& sha);
    void LinkNewFile(Sync::LocalFile local);
    void DetachCopy(const QString& pathKey);
    void ProcessGone();
    QString NewFileUid(const QByteArray& sha) const;
    Sync::TrackInfo InfoOf(quint32 trackId) const;
    QString OnlineUidOf(quint32 trackId) const;
    bool IsCurrentTrackFile(const QString& pathKey) const;
    /// The deepest library folder (or the placement folder) holding `path`; empty when none does.
    QString RootOf(const QString& path) const;
    void DropCandidate(const QString& pathKey);
    void Unpark(const QString& key);
    /// Whether the file still has the size and modification time it was indexed with.
    bool IsUnchangedOnDisk(const Sync::LocalFile& local) const;
    /// Opens a file for reading without stopping others from renaming or deleting it.
    static bool OpenForReading(QFile& file, const QString& path);
    /// SHA-256 of a file read in chunks; empty when it cannot be read, is cancelled or is not `expectedSize` bytes
    /// long (-1: any size).
    static QByteArray HashFile(const QString& path, qint64 expectedSize, const std::atomic_bool& cancel);

    // Files: transfers (SyncEngineTransfers.cpp)
    QString TempDir() const;
    QString PartPath(const QByteArray& sha) const;
    void CloseServed(const QString& pathKey);
    /// Counts a failed download of `key` and holds it back for a while.
    void Backoff(const QString& key);
    /// Brings the outdated local copies of a held File item to its current content.
    void UpgradeCopies(const QString& key);
    /// Replaces a local copy of `record` with `source`, which is moved or copied in.
    ReplaceResult ReplaceCopy(const QString& pathKey, const QString& source, bool bMove, const Sync::Record& record);
    /// Sets the record's modification time on a placed file and indexes it as a copy.
    void AdoptPlaced(const QString& path, const Sync::Record& record);
    void CleanTemp();
    void SchedulePump();
    void PumpDownloads();
    void StartLocalCopy(const QString& key, const QString& sourcePath);
    void StartDownload(const QString& key, Link& link);
    void RequestBlocks(Download& download, Link& link);
    void OnBlockData(const QString& peerId, const soundlink::sync::v1::BlockData& data);
    void AbortDownload(const QString& key, bool bKeepPart);
    void VerifyDownload(const QString& key);
    /// A background copy or check of a download finished; `bMatch`: the content has the expected checksum.
    void OnContentChecked(const QString& key, const std::shared_ptr<std::atomic_bool>& cancel, bool bMatch);
    /// Places a verified download; false while it has to wait.
    bool Finalize(const QString& key);
    /// Before a local file is replaced or trashed: keeps its content if another item still needs it.
    void KeepOldContent(const Sync::LocalFile& local);
    void ApplyFileTombstone(const QString& key, const QString& peerId);
    void RetryDeferred();
    void ServeBlock(const QString& peerId, const soundlink::sync::v1::BlockRequest& request);
    void Housekeeping();

    // Library (SyncEngineLibrary.cpp)
    void RunLibraryPass();
    /// Commits unsynced library changes of one Online / Meta / Playlist key, so a remote record merges against them.
    void FlushLocal(const QString& key);
    void RunPlaylistPass();
    void BridgeOnline();
    void BridgeMeta();
    void BridgePlaylists();
    /// Commits the saved state of the online track of an Online key, or brings the track in line with the record.
    void BridgeOnlineKey(const QString& key, quint32 trackId);
    /// Commits the labels and tag edits of a track, or brings them in line with the record.
    void BridgeMetaKey(const QString& key, quint32 trackId);
    /// Commits the local playlist of a Playlist key, or brings it in line with the record; `bFlush` only commits.
    void BridgePlaylistKey(const QString& key, bool bFlush);
    void ApplyOnline(const QString& key);
    void ApplyMeta(const QString& key);
    /// Writes the record into the local playlist; without `bForce` only when the playlist has no unsynced edits.
    void ApplyPlaylist(const QString& key, bool bForce);
    /// Uid of the File or Online item a track is; empty when it has none yet.
    QString TrackUid(const Track& track) const;
    /// The track that speaks for a File or Online uid here (never a transient online track); 0 when there is none.
    quint32 TrackIdForUid(const QString& uid) const;
    void EnsureUidIndex() const;
    /// Track id -> the uid it stood for in a playlist record (empty: it had none).
    using MemberUids = QHash<quint32, QString>;
    /// Record body of a local playlist. Uids of `previous` that never were local members stay where they were;
    /// `members` gets the uid each member is written as.
    Sync::PlaylistBody BodyOf(const Playlist& playlist, const Sync::PlaylistBody* previous, const MemberUids& known,
                              MemberUids& members) const;
    /// Writes a record into `playlist`, keeping members it cannot name; true when the playlist changed.
    bool ApplyBodyTo(const Sync::Record& record, const MemberUids& known, Playlist& playlist, MemberUids& members) const;
    /// Records which library state an Online / Meta / Playlist record matches (localSeen); schedules a save when it changes.
    void SetSeen(const QString& key, const QByteArray& seen);

    Sync::State state;
    QString statePath;
    QHash<QString, Link> links;
    QHash<QString, std::shared_ptr<Download>> downloads;  ///< By key.
    QHash<QString, Candidate> candidates;                 ///< By PathKey.
    QHash<QString, qint64> goneSince;                     ///< By PathKey: indexed files the library stopped listing.
    QHash<QByteArray, std::shared_ptr<ServedFile>> served;
    QMultiHash<QByteArray, QString> keysBySha;            ///< Content of alive File records -> key.
    QMultiHash<QByteArray, QString> keysByOldSha;         ///< Content of File tombstones and history -> key.
    QMultiHash<QByteArray, QString> localBySha;           ///< Verified content -> PathKey.
    QMultiHash<QString, QString> filesByUid;              ///< File uid -> PathKey of local copies.
    QSet<QString> needContent;                            ///< Alive File keys this device does not hold.
    QSet<QString> deferred;                               ///< Keys waiting for the player to leave their file.
    QHash<QString, int> failures;                         ///< Key -> failed downloads in a row.
    QHash<QString, qint64> retryAtMs;                     ///< Key -> not downloaded or placed before this time.
    QHash<QString, Sync::LocalFile> detached;             ///< By PathKey: copies that left with their folder, relinked unread when they return.
    mutable QHash<QString, quint32> uidIndex;             ///< File / Online uid -> library track id.
    mutable QMultiHash<QString, QString> copiesByOnline;  ///< Online uid -> PathKey of local copies of its File items.
    mutable QHash<quint32, QString> trackUids;            ///< Local (not online) track id -> TrackUid(), built with uidIndex.
    mutable quint64 uidIndexRevision = 0;                 ///< Library structure revision uidIndex was built for.
    mutable quint64 uidIndexGeneration = 0;               ///< Changes whenever uidIndex does.
    mutable bool bUidIndexDirty = true;
    quint64 playlistsGeneration = 0;                      ///< uidIndexGeneration in-sync playlists were last brought in line with.
    qint64 lastTempCleanupMs = 0;
    QHash<QString, int> trashedBy;                        ///< Peer -> files trashed since the last notification.
    QSet<QString> conflictedKeys;                         ///< Keys a conflict holds back: Conflict::key, or otherKey of a Name conflict.
    QStringList roots;
    QString placementRoot;
    QString blockedReason;
    qint64 blockedSinceMs = 0;
    int activeHashes = 0;
    int newConflicts = 0;
    quint32 nextRequestId = 0;
    bool bStarted = false;
    bool bLibraryReady = false;
    bool bStartupPass = true;
    bool bConfirmRemote = false;
    bool bConflictsDirty = false;

    QTimer saveTimer;
    QTimer pushTimer;
    QTimer libraryTimer;
    QTimer playlistTimer;
    QTimer pumpTimer;
    QTimer progressTimer;
    QTimer housekeepingTimer;
    QThreadPool pool;  ///< Hashing, local copies and checks of received files.
};
