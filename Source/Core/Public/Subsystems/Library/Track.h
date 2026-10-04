//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include <QDateTime>
#include <QHashFunctions>
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
    QString path;       ///< Absolute path.
    qint64 size = 0;    ///< Size in bytes at the last scan.
    QDateTime modified; ///< Modification time at the last scan.
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

/// @brief What the user curates: free-form tags for smart playlists.
struct UserComponent
{
    QStringList labels;
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

    /// @brief A scanned file or the downloaded copy of an online track. It may have gone missing since.
    bool HasLocalFile() const { return !file.path.isEmpty(); }

    /// @brief Local, or a saved online track.
    ///
    /// Transient online tracks exist only for playback and search results: not listed, not cached.
    bool IsInLibrary() const { return !IsOnline() || stream.bSaved; }
};
