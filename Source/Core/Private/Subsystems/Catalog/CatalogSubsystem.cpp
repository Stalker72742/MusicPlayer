//
// Created by Stalker7274 on 08.11.2026.
//

#include "Catalog/CatalogSubsystem.h"

#include <QCollator>
#include <QRegularExpression>

#include <algorithm>
#include <climits>
#include <numeric>

#include "Library/LibrarySubsystem.h"

namespace
{
    constexpr int RebuildDelayMs = 200;
    constexpr int MaxImageNameLength = 120;

    const QString VariousArtists = QStringLiteral("Various artists");

    QString ImageFileName(const QString& text)
    {
        QString name;
        name.reserve(text.size());
        for (const QChar c : text)
        {
            if (c.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(c))
                name.append(QLatin1Char('_'));
            else
                name.append(c);
        }
        name = name.simplified();
        while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
            name.chop(1);
        return name.left(MaxImageNameLength);
    }

    QStringList Split(const QString& tag, const QRegularExpression& separators)
    {
        QStringList names;
        QStringList keys;
        for (const QString& part : tag.split(separators, Qt::SkipEmptyParts))
        {
            const QString name = part.simplified();
            const QString key = CatalogSubsystem::MakeKey(name);
            if (!name.isEmpty() && !keys.contains(key))
            {
                names.append(name);
                keys.append(key);
            }
        }
        return names;
    }

    /// Collects one entry while the index is built.
    struct Accumulator
    {
        QHash<QString, int> spellings;
        QString artist;
        QList<TrackId> tracks;
        QList<int> trackNumbers;
        QHash<int, int> years;
        QDateTime lastAdded;

        void Add(const QString& name, const Track& track)
        {
            ++spellings[name];
            tracks.append(track.id);
            trackNumbers.append(track.tags.trackNumber);
            if (track.tags.year > 0)
                ++years[track.tags.year];
            if (!lastAdded.isValid() || track.stats.dateAdded > lastAdded)
                lastAdded = track.stats.dateAdded;
        }

        QString MostCommonName() const
        {
            QString best;
            int bestCount = 0;
            for (auto it = spellings.cbegin(); it != spellings.cend(); ++it)
            {
                if (it.value() > bestCount || (it.value() == bestCount && it.key() < best))
                {
                    best = it.key();
                    bestCount = it.value();
                }
            }
            return best;
        }
    };

    template <typename Index>
    void Finish(Index& index, CatalogKind kind, QHash<QString, Accumulator>& accumulators, const QStringList& order)
    {
        index.entries.clear();
        index.rowByKey.clear();

        for (const QString& key : order)
        {
            Accumulator& acc = accumulators[key];
            CatalogEntry entry;
            entry.kind = kind;
            entry.key = key;
            entry.name = acc.MostCommonName();
            entry.artist = acc.artist;
            entry.lastAdded = acc.lastAdded;

            int bestYearCount = 0;
            for (auto it = acc.years.cbegin(); it != acc.years.cend(); ++it)
            {
                if (it.value() > bestYearCount)
                {
                    entry.year = it.key();
                    bestYearCount = it.value();
                }
            }

            if (kind == CatalogKind::Album)
            {
                QList<qsizetype> rows(acc.tracks.size());
                std::iota(rows.begin(), rows.end(), 0);
                std::stable_sort(rows.begin(), rows.end(), [&acc](qsizetype a, qsizetype b) {
                    const int na = acc.trackNumbers[a] > 0 ? acc.trackNumbers[a] : INT_MAX;
                    const int nb = acc.trackNumbers[b] > 0 ? acc.trackNumbers[b] : INT_MAX;
                    return na < nb;
                });
                for (qsizetype row : rows)
                    entry.tracks.append(acc.tracks[row]);
                entry.imageName = ImageFileName(entry.artist + QStringLiteral(" - ") + entry.name);
            }
            else
            {
                entry.tracks = acc.tracks;
                entry.imageName = ImageFileName(entry.name);
            }
            index.entries.append(entry);
        }

        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(index.entries.begin(), index.entries.end(), [&collator](const CatalogEntry& a, const CatalogEntry& b) {
            return collator.compare(a.name, b.name) < 0;
        });

        for (qsizetype row = 0; row < index.entries.size(); ++row)
            index.rowByKey.insert(index.entries[row].key, row);
    }

    QString PrimaryArtist(const Track& track)
    {
        const QStringList artists = CatalogSubsystem::SplitArtists(track.tags.artist);
        return artists.isEmpty() ? QString() : artists.first();
    }
}

QString CatalogKindId(CatalogKind kind)
{
    switch (kind)
    {
        case CatalogKind::Artist: return QStringLiteral("artist");
        case CatalogKind::Album:  return QStringLiteral("album");
        case CatalogKind::Genre:  return QStringLiteral("genre");
    }
    return {};
}

std::optional<CatalogKind> ParseCatalogKind(const QString& id)
{
    if (id == QLatin1String("artist"))
        return CatalogKind::Artist;
    if (id == QLatin1String("album"))
        return CatalogKind::Album;
    if (id == QLatin1String("genre"))
        return CatalogKind::Genre;
    return std::nullopt;
}

CatalogSubsystem::CatalogSubsystem()
{
    rebuildTimer.setSingleShot(true);
    rebuildTimer.setInterval(RebuildDelayMs);
    connect(&rebuildTimer, &QTimer::timeout, this, &CatalogSubsystem::Rebuild);
    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, &rebuildTimer, qOverload<>(&QTimer::start));

    Rebuild();
}

const CatalogSubsystem::Index& CatalogSubsystem::GetIndex(CatalogKind kind) const
{
    switch (kind)
    {
        case CatalogKind::Album: return albums;
        case CatalogKind::Genre: return genres;
        default:                 return artists;
    }
}

const QList<CatalogEntry>& CatalogSubsystem::GetEntries(CatalogKind kind) const
{
    return GetIndex(kind).entries;
}

const CatalogEntry* CatalogSubsystem::Find(CatalogKind kind, const QString& key) const
{
    const Index& index = GetIndex(kind);
    const auto it = index.rowByKey.constFind(key);
    return it == index.rowByKey.cend() ? nullptr : &index.entries[*it];
}

QStringList CatalogSubsystem::GetNames(CatalogKind kind) const
{
    QStringList names;
    for (const CatalogEntry& entry : GetIndex(kind).entries)
        names.append(entry.name);
    return names;
}

QStringList CatalogSubsystem::GetKeysOf(CatalogKind kind, TrackId track) const
{
    return GetIndex(kind).keysByTrack.value(track);
}

QString CatalogSubsystem::MakeKey(const QString& name)
{
    return name.simplified().toCaseFolded();
}

QStringList CatalogSubsystem::SplitArtists(const QString& tag)
{
    static const QRegularExpression separators(
        QStringLiteral(R"(\s*[,;]\s*|\s+(?:feat\.?|ft\.|featuring)\s+)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression topic(QStringLiteral(R"(\s+-\s+Topic$)"));

    QString text = tag;
    text.remove(topic);
    return Split(text, separators);
}

QStringList CatalogSubsystem::SplitGenres(const QString& tag)
{
    static const QRegularExpression separators(QStringLiteral(R"(\s*[,;/]\s*)"));
    return Split(tag, separators);
}

void CatalogSubsystem::Rebuild()
{
    QHash<QString, Accumulator> artistAcc;
    QHash<QString, Accumulator> albumAcc;
    QHash<QString, Accumulator> genreAcc;
    QStringList artistOrder;
    QStringList albumOrder;
    QStringList genreOrder;

    Index newArtists;
    Index newAlbums;
    Index newGenres;

    // Albums are grouped by name first: a name whose tracks mostly share an artist is one album per artist
    // (two artists' "Greatest Hits" stay apart), otherwise it is a compilation of various artists.
    QHash<QString, QList<const Track*>> tracksByAlbum;
    QStringList albumNames;

    for (const Track& track : LibrarySubsystem::Get().GetTracks())
    {
        if (!track.IsInLibrary())
            continue;

        for (const QString& artist : SplitArtists(track.tags.artist))
        {
            const QString key = MakeKey(artist);
            if (!artistAcc.contains(key))
                artistOrder.append(key);
            artistAcc[key].Add(artist, track);
            newArtists.keysByTrack[track.id].append(key);
        }

        for (const QString& genre : SplitGenres(track.tags.genre))
        {
            const QString key = MakeKey(genre);
            if (!genreAcc.contains(key))
                genreOrder.append(key);
            genreAcc[key].Add(genre, track);
            newGenres.keysByTrack[track.id].append(key);
        }

        const QString album = track.tags.album.simplified();
        if (!album.isEmpty())
        {
            const QString nameKey = MakeKey(album);
            if (!tracksByAlbum.contains(nameKey))
                albumNames.append(nameKey);
            tracksByAlbum[nameKey].append(&track);
        }
    }

    for (const QString& nameKey : albumNames)
    {
        const QList<const Track*>& group = tracksByAlbum[nameKey];

        QHash<QString, int> artistCounts;
        QHash<QString, QString> artistNames;
        for (const Track* track : group)
        {
            const QString artist = PrimaryArtist(*track);
            ++artistCounts[MakeKey(artist)];
            artistNames.insert(MakeKey(artist), artist);
        }
        QString topArtist;
        int topCount = 0;
        for (auto it = artistCounts.cbegin(); it != artistCounts.cend(); ++it)
        {
            if (!it.key().isEmpty() && it.value() > topCount)
            {
                topArtist = it.key();
                topCount = it.value();
            }
        }
        const bool bCompilation = artistCounts.size() > 1 && topCount * 2 < group.size();

        for (const Track* track : group)
        {
            QString albumArtist;
            if (bCompilation)
                albumArtist = VariousArtists;
            else
            {
                const QString own = PrimaryArtist(*track);
                albumArtist = own.isEmpty() ? artistNames.value(topArtist) : own;
            }

            const QString key = nameKey + QChar(0x1F) + MakeKey(albumArtist);
            if (!albumAcc.contains(key))
            {
                albumOrder.append(key);
                albumAcc[key].artist = albumArtist;
            }
            albumAcc[key].Add(track->tags.album.simplified(), *track);
            newAlbums.keysByTrack[track->id].append(key);
        }
    }

    Finish(newArtists, CatalogKind::Artist, artistAcc, artistOrder);
    Finish(newAlbums, CatalogKind::Album, albumAcc, albumOrder);
    Finish(newGenres, CatalogKind::Genre, genreAcc, genreOrder);

    const bool bChanged = newArtists.entries != artists.entries || newAlbums.entries != albums.entries
        || newGenres.entries != genres.entries;

    artists = std::move(newArtists);
    albums = std::move(newAlbums);
    genres = std::move(newGenres);

    if (bChanged)
        emit catalogChanged();
}
