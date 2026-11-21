//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include <QDateTime>
#include <QHashFunctions>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <compare>

/// @file
/// @brief A track is an id plus plain component structs.
///
/// Each system writes only its own component: the scanner FileComponent, the metadata reader TagsComponent,
/// the player and the UI StatsComponent, the user UserComponent.
///
/// Identity is the TrackId, not where the audio comes from: an online track that gets a local copy keeps its id,
/// so likes, tags and playlists stay. Playback prefers a local file and falls back to the stream.

/// @brief Stable id of a track, kept between runs. 0 is invalid.
struct TrackId
{
    quint32 value = 0;

    /// @brief Whether this refers to a track at all.
    bool IsValid() const { return value != 0; }

    auto operator<=>(const TrackId&) const = default;
};

/// @brief Hash for QHash / QSet keys.
inline size_t qHash(TrackId id, size_t seed = 0)
{
    return qHash(id.value, seed);
}

Q_DECLARE_METATYPE(TrackId)

/// @brief A local audio file: a scanned one, or the downloaded copy of an online track.
struct FileComponent
{
    QString path;          ///< Absolute path. Stays when the file goes missing, so it is recognized when it returns.
    qint64 size = 0;       ///< Size in bytes at the last scan.
    qint64 modifiedMs = 0; ///< Modification time at the last scan, ms since the epoch.
    quint64 fileId = 0;    ///< File id on its volume, follows the file through renames and moves. 0 if unknown.
    quint32 volumeId = 0;  ///< Serial number of the volume of fileId.
    bool bMissing = false; ///< Not found by the last look at the disk: the folder may be gone or the drive unplugged.
};

/// @brief Tags and duration, read from the file or given by the online source.
struct TagsComponent
{
    QString title;
    QString artist;
    QString album;
    QString genre;
    int trackNumber = 0;
    int year = 0;
    qint64 durationMs = 0;

    /// @brief Read from the file; false while only the guesses from the file name are known.
    bool bFromFile = false;
};

/// @brief Listening statistics, written by the player and the UI.
struct StatsComponent
{
    QDateTime dateAdded;
    QDateTime lastPlayed;
    int playCount = 0;
    bool bLiked = false;
};

/// @brief What the user curates: free-form tags for smart playlists and their own values of the track's tags.
struct UserComponent
{
    QStringList labels;
    /// Tag values set in the app, by `title`, `artist`, `album`, `genre`, `year`, `track`. They win over what the
    /// file or the online source says, and the audio file itself is not changed.
    QMap<QString, QString> tagEdits;
};

/// @brief Source page of an online track.
///
/// Online tracks (found through yt-dlp etc.) come from a page: without a local copy the audio stream is
/// resolved from the page URL every time the track is played. Search results are transient until liked,
/// added to the library or to a playlist.
struct StreamComponent
{
    QString pageUrl;      ///< Page the track was found on; empty for local tracks.
    QString thumbnailUrl; ///< Cover image.
    bool bSaved = false;  ///< In the library rather than transient.
    QString channelUrl;   ///< Channel page of the author; empty if not known yet.
};

/// @brief A library track: its id and all of its components.
struct Track
{
    TrackId id;
    FileComponent file;
    TagsComponent tags;
    StatsComponent stats;
    UserComponent user;
    StreamComponent stream;

    /// @brief Came from a web page (it may still have a local copy).
    bool IsOnline() const { return !stream.pageUrl.isEmpty(); }

    /// @brief A scanned file or the downloaded copy of an online track that was there at the last look at the disk.
    bool HasLocalFile() const { return !file.path.isEmpty() && !file.bMissing; }

    /// @brief An online track nobody saved: exists only for playback and search results, not listed, not stored.
    bool IsTransient() const { return IsOnline() && !stream.bSaved; }

    /// @brief What the user sees in the lists: a local track whose file is there, or a saved online track.
    ///
    /// A local track whose file went missing stays registered with its likes, tags and playlist places,
    /// but is hidden everywhere until the file returns.
    bool IsInLibrary() const { return IsOnline() ? stream.bSaved : !file.bMissing; }
};
