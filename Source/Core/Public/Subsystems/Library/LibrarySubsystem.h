//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <memory>

#include "Library/Track.h"

class FolderWatcher;
class MetadataReader;

/// @brief The track registry: everything known about every track, addressed by TrackId.
///
/// Playlists and the player hold only TrackIds. Stored in `<AppLocalData>/library.bin` (`library.json` of older
/// versions is imported once). The file is loaded first, so the library is available immediately; a background scan
/// then brings it up to date, and FolderWatcher keeps it so while the app runs: changed paths are looked at within
/// a fraction of a second, without a full scan.
///
/// A file that went away is not forgotten: its track stays with FileComponent::bMissing, hidden from the lists.
/// When a new file turns up, its file id (or size, time and name) finds the missing track it really is, so renaming
/// or moving a file keeps its TrackId, and with it the likes, tags and playlist places.
///
/// Music folders live in the Settings config (AppConfigs::SettingsKeys::MusicScanFolders).
///
/// Online tracks (see StreamComponent) live next to the files: saved ones are cached by page URL,
/// transient ones (search results, single playback) only exist until RemoveTransientTracks().
class LibrarySubsystem : public Subsystem<LibrarySubsystem>
{
    Q_OBJECT
    friend class Subsystem<LibrarySubsystem>;

public:
    /// @brief Loads the cache and starts a scan. Call once at startup, after the configs are registered.
    void Initialize();

    /// @brief Scans the music folders again (queued if a scan is already running).
    void Rescan();

    /// @brief The music folders from the Settings config.
    QStringList GetFolders() const;
    /// @brief Replaces the music folders (normalized, without duplicates) and rescans if they changed.
    void SetFolders(const QStringList& folders);
    /// @brief Adds a music folder; see SetFolders().
    void AddFolder(const QString& folder);
    /// @brief Removes a music folder; see SetFolders().
    void RemoveFolder(const QString& folder);
    /// @brief Restores the default music folders and rescans.
    void ResetFolders();
    /// @brief Whether the music folders differ from the default.
    bool AreFoldersModified() const;

    /// @brief Every track, in no particular order.
    ///
    /// Swap-removal reorders the list, so keep TrackIds rather than indices.
    /// Includes transient online tracks: check Track::IsInLibrary() before listing.
    const QList<Track>& GetTracks() const { return tracks; }
    /// @brief The track with this id, or nullptr.
    const Track* FindTrack(TrackId id) const;

    /// @brief Finds the online track with this page URL or creates a new transient one.
    ///
    /// Tags and thumbnail of a transient track are refreshed; a saved track keeps what the user has.
    /// @param pageUrl Page the track was found on; the identity of an online track.
    /// @param tags Tags known from the source.
    /// @param thumbnailUrl Cover image URL, may be empty.
    /// @param channelUrl Channel page of the author, may be empty; a saved track takes it if it had none.
    /// @return Id of the found or created track.
    TrackId AddOnlineTrack(const QString& pageUrl, const TagsComponent& tags, const QString& thumbnailUrl,
        const QString& channelUrl = {});
    /// @brief The online track with this page URL, or an invalid id.
    TrackId FindOnlineTrack(const QString& pageUrl) const;

    /// @brief Saves an online track to the library or makes it transient again.
    ///
    /// A saved track is listed and cached; unsaving also unlikes it. Does nothing for local tracks.
    void SetSaved(TrackId id, bool bSaved);

    /// @brief Sets the downloaded copy of an online track, played instead of the stream.
    ///
    /// The track is saved too; an empty path goes back to streaming. Everything else about the track stays.
    /// The file itself is the caller's: it is neither moved nor deleted here, and scanning a music folder
    /// that holds it finds this track.
    /// @param id An online track.
    /// @param path Path of the local copy, or empty.
    void SetLocalFile(TrackId id, const QString& path);

    /// @brief The file of a track was renamed or moved by the app: the track follows it at once.
    ///
    /// The folder watcher would find the move too, a moment later. Tags not read from the file yet are read again,
    /// so a title guessed from the file name follows the new name.
    /// @param id A track with a local file (scanned, or the downloaded copy of an online track).
    /// @param path The new path of the file.
    void SetFilePath(TrackId id, const QString& path);

    /// @brief The file of a local track was deleted by the app: the track is hidden at once, as any missing file.
    ///
    /// Likes, tags and playlist places stay, so a file restored from the Recycle Bin brings the track back.
    void MarkFileGone(TrackId id);

    /// @brief Sets the user's values of a track's tags; other fields stay as they are.
    /// @param edits By `title`, `artist`, `album`, `genre`, `year`, `track`; an empty title is ignored.
    void SetTagEdits(TrackId id, const QMap<QString, QString>& edits);
    /// @brief Makes the title follow the file name, and the artist too if the name is "Artist - Title".
    ///
    /// For a file renamed in the app: without this a title tag inside the file would keep showing the old name.
    void TakeTagsFromFileName(TrackId id);

    /// @brief Replaces the user's tags of a track (used by smart playlists).
    void SetLabels(TrackId id, const QStringList& labels);

    /// @brief Drops transient online tracks.
    /// @param keep Tracks still in use (e.g. the playing one) that stay.
    void RemoveTransientTracks(const QSet<TrackId>& keep);

    /// @brief Likes or unlikes a track. Liking an online track also saves it to the library.
    void SetLiked(TrackId id, bool bLiked);
    /// @brief Bumps the play count and the last played time.
    void MarkPlayed(TrackId id);

    /// @brief Stores the exact duration, known to the player once a file is loaded.
    void SetDuration(TrackId id, qint64 durationMs);

    /// @brief Whether a full scan of the folders is running. Looks at single changed paths do not count.
    bool IsScanning() const { return bFullScanRunning; }
    /// @brief Changes whenever the set of listed tracks or the order of a path-sorted list may have changed.
    quint64 GetStructureRevision() const { return structureRevision; }
    /// @brief Number of files whose tags are still to be read.
    int GetPendingMetadataCount() const;

signals:
    /// @brief Tracks were added, removed or changed. Coalesced: at most a few times per second while scanning.
    void tracksChanged();
    /// @brief The music folders changed.
    void foldersChanged();
    /// @brief A scan started or finished, or metadata reading went idle.
    void scanStateChanged();

private:
    LibrarySubsystem();

    void Deinitialize() override;

    /// One look at the disk: what was found, and where a track that was not found counts as gone.
    struct ScanJob;

    void StartNextScan();
    void OnScanFinished();
    void ApplyScan(const ScanJob& job);
    /// A file being copied reports many changes: waits for a pause, but not forever.
    void OnPathsChanged(const QStringList& paths);
    void OnTagsRead(TrackId id, const TagsComponent& tags, bool bSuccess);
    static void ApplyTagEdits(Track& track);

    Track* FindMutable(TrackId id);
    TrackId AddTrack(const QString& path, qint64 size, qint64 modifiedMs, quint64 fileId, quint32 volumeId);
    void RemoveTrack(TrackId id);
    void IndexLoadedTracks(QList<Track> loaded);
    static TagsComponent GuessTagsFromPath(const QString& path);

    void NotifyChanged();
    void ScheduleSave();

    QString GetDataPath(const QString& fileName) const;
    /// An unreadable registry is moved aside rather than overwritten: it holds the only copy of likes and play counts.
    void LoadRegistry();
    void SaveRegistry();
    void ApplyFoldersChanged();

    QList<Track> tracks;
    QHash<TrackId, qsizetype> indexById;
    QHash<QString, TrackId> idByPath;
    QHash<QString, TrackId> idByUrl;
    quint32 nextId = 1;
    quint64 structureRevision = 0;

    MetadataReader* metadataReader = nullptr;
    FolderWatcher* folderWatcher = nullptr;

    QFutureWatcher<void> scanWatcher;
    std::shared_ptr<ScanJob> currentJob;
    std::atomic_bool bStopScan{false};
    bool bFullScanRunning = false;
    bool bFullScanPending = false;
    QSet<QString> pendingPaths;
    QElapsedTimer pendingSince;
    QTimer pathsTimer;

    QTimer notifyTimer;
    QTimer saveTimer;
    bool bInitialized = false;
};
