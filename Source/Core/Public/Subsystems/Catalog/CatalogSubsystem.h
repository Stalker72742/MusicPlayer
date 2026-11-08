//
// Created by Stalker7274 on 08.11.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <optional>

#include "Library/Track.h"

/// @brief What a catalog entry groups tracks by.
enum class CatalogKind
{
    Artist,
    Album,
    Genre
};

/// @brief `"artist"`, `"album"` or `"genre"`: the id of a kind in navigation, QML and settings.
QString CatalogKindId(CatalogKind kind);
/// @brief The kind of an id from CatalogKindId(), nothing for anything else.
std::optional<CatalogKind> ParseCatalogKind(const QString& id);

/// @brief An artist, album or genre the library tracks name, with its tracks.
struct CatalogEntry
{
    CatalogKind kind = CatalogKind::Artist;
    QString key;          ///< Stable id: case-folded name (albums: name and album artist).
    QString name;         ///< As most of its tracks spell it.
    QString artist;       ///< Albums: the album artist, or "Various artists".
    QString imageName;    ///< File name of its picture without extension (ArtworkSubsystem).
    QList<TrackId> tracks; ///< Albums in track-number order, others in library order.
    int year = 0;         ///< Albums: the most common year, 0 if unknown.
    QDateTime lastAdded;  ///< When the newest of its tracks was added.

    bool operator==(const CatalogEntry&) const = default;
};

/// @brief Artists, albums and genres of the library, derived from track tags.
///
/// Follows LibrarySubsystem: every change of the track set or of tags (a file registered, tags read or
/// edited) rebuilds the index shortly after, so entries appear and disappear with their tracks. Only tracks
/// in the library count (IsInLibrary()). Tags with several names are split: artists on `,` `;` `feat.`
/// `ft.`, genres on `,` `;` `/`. A YouTube "Artist - Topic" channel counts as "Artist".
/// Entries hold no data of their own, so nothing is stored; pictures belong to ArtworkSubsystem.
class CatalogSubsystem : public Subsystem<CatalogSubsystem>
{
    Q_OBJECT
    friend class Subsystem<CatalogSubsystem>;

public:
    /// @brief Entries of a kind sorted by name.
    const QList<CatalogEntry>& GetEntries(CatalogKind kind) const;
    /// @brief The entry with a key, nullptr if there is none.
    const CatalogEntry* Find(CatalogKind kind, const QString& key) const;
    /// @brief Names of a kind sorted by name, e.g. for autocompletion.
    QStringList GetNames(CatalogKind kind) const;

    /// @brief Keys of the entries a track belongs to (several artists or genres possible).
    QStringList GetKeysOf(CatalogKind kind, TrackId track) const;

    /// @brief Key of a name: whitespace collapsed, case folded.
    static QString MakeKey(const QString& name);
    /// @brief Names of the artists of an artist tag.
    static QStringList SplitArtists(const QString& tag);
    /// @brief Names of the genres of a genre tag.
    static QStringList SplitGenres(const QString& tag);

signals:
    /// @brief The entries changed; emitted after a rebuild that changed anything.
    void catalogChanged();

private:
    CatalogSubsystem();

    void Rebuild();

    struct Index
    {
        QList<CatalogEntry> entries;
        QHash<QString, qsizetype> rowByKey;
        QHash<TrackId, QStringList> keysByTrack;
    };

    const Index& GetIndex(CatalogKind kind) const;

    Index artists;
    Index albums;
    Index genres;
    QTimer rebuildTimer;
};
