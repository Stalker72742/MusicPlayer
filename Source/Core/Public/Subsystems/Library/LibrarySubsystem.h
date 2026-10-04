//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "Library/Track.h"

class MetadataReader;

/// @brief The music library: every audio file found in the music folders plus online tracks.
///
/// Cached in `<AppLocalData>/library.json`. The cache is loaded first, so the library is available immediately;
/// a background scan then adds new files, drops missing ones and queues changed ones for metadata reading.
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
    /// @return Id of the found or created track.
    TrackId AddOnlineTrack(const QString& pageUrl, const TagsComponent& tags, const QString& thumbnailUrl);
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

    /// @brief Whether a folder scan is running.
    bool IsScanning() const { return scanWatcher.isRunning(); }
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

    struct ScannedFile
    {
        QString path;
        qint64 size = 0;
        QDateTime modified;
    };

    static QList<ScannedFile> ScanFolders(const QStringList& folders);

    void OnScanFinished();
    void OnTagsRead(TrackId id, const TagsComponent& tags, bool bSuccess);

    Track* FindMutable(TrackId id);
    TrackId AddTrack(const ScannedFile& file);
    void RemoveTrack(TrackId id);
    static TagsComponent GuessTagsFromPath(const QString& path);

    void NotifyChanged();
    void ScheduleSave();

    QString GetCachePath() const;
    void LoadCache();
    void SaveCache();

    QList<Track> tracks;
    QHash<TrackId, qsizetype> indexById;
    QHash<QString, TrackId> idByPath;
    QHash<QString, TrackId> idByUrl;
    quint32 nextId = 1;

    MetadataReader* metadataReader = nullptr;
    QFutureWatcher<QList<ScannedFile>> scanWatcher;
    bool bRescanPending = false;

    QTimer notifyTimer;
    QTimer saveTimer;
    bool bInitialized = false;
};
