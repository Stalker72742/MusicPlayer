//
// Created by Stalker7274 on 04.10.2026.
//

#include "Playlists/PlaylistSubsystem.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

#include "Library/LibrarySubsystem.h"

namespace
{
    constexpr int StoreVersion = 1;
    constexpr int SaveDelayMs = 1000;

    QJsonArray ToJson(const QList<TrackId>& ids)
    {
        QJsonArray array;
        for (TrackId id : ids)
            array.append(static_cast<qint64>(id.value));
        return array;
    }

    QList<TrackId> TracksFromJson(const QJsonArray& array)
    {
        QList<TrackId> ids;
        for (const QJsonValue& value : array)
        {
            const TrackId id{static_cast<quint32>(value.toInteger())};
            if (id.IsValid() && !ids.contains(id))
                ids.append(id);
        }
        return ids;
    }

    bool TextMatches(const QString& haystack, const QString& op, const QString& value)
    {
        const QString hay = haystack.toLower();
        const QString needle = value.trimmed().toLower();

        if (op == QStringLiteral("contains"))
            return hay.contains(needle);
        if (op == QStringLiteral("notContains"))
            return !hay.contains(needle);
        if (op == QStringLiteral("is"))
            return hay == needle;
        if (op == QStringLiteral("isNot"))
            return hay != needle;
        if (op == QStringLiteral("startsWith"))
            return hay.startsWith(needle);
        return false;
    }

    bool NumberMatches(double number, const QString& op, const QString& value)
    {
        bool bOk = false;
        const double target = value.trimmed().replace(',', '.').toDouble(&bOk);
        if (!bOk)
            return false;

        if (op == QStringLiteral("is"))
            return qFuzzyCompare(number + 1, target + 1);
        if (op == QStringLiteral("isNot"))
            return !qFuzzyCompare(number + 1, target + 1);
        if (op == QStringLiteral("gt"))
            return number > target;
        if (op == QStringLiteral("lt"))
            return number < target;
        return false;
    }

    bool DateMatches(const QDateTime& date, const QString& op, const QString& value)
    {
        bool bOk = false;
        const int days = value.trimmed().toInt(&bOk);
        if (!bOk)
            return false;

        const bool bRecent = date.isValid() && date.daysTo(QDateTime::currentDateTime()) < days;
        if (op == QStringLiteral("inLast"))
            return bRecent;
        if (op == QStringLiteral("notInLast"))
            return !bRecent;
        return false;
    }

    QString SourceOf(const Track& track)
    {
        if (!track.IsOnline())
            return QStringLiteral("local");
        return track.HasLocalFile() ? QStringLiteral("downloaded") : QStringLiteral("youtube");
    }
}

PlaylistSubsystem::PlaylistSubsystem()
{
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(SaveDelayMs);
    connect(&saveTimer, &QTimer::timeout, this, &PlaylistSubsystem::Save);

    Load();
}

void PlaylistSubsystem::Deinitialize()
{
    if (saveTimer.isActive())
    {
        saveTimer.stop();
        Save();
    }
}

const QList<PlaylistSubsystem::RuleField>& PlaylistSubsystem::GetRuleFields()
{
    static const QList<RuleField> fields{
        {QStringLiteral("artist"), QStringLiteral("Artist"), QStringLiteral("text")},
        {QStringLiteral("title"), QStringLiteral("Title"), QStringLiteral("text")},
        {QStringLiteral("album"), QStringLiteral("Album"), QStringLiteral("text")},
        {QStringLiteral("genre"), QStringLiteral("Genre"), QStringLiteral("text")},
        {QStringLiteral("tag"), QStringLiteral("Tag"), QStringLiteral("text")},
        {QStringLiteral("year"), QStringLiteral("Year"), QStringLiteral("number")},
        {QStringLiteral("duration"), QStringLiteral("Length, min"), QStringLiteral("number")},
        {QStringLiteral("playCount"), QStringLiteral("Plays"), QStringLiteral("number")},
        {QStringLiteral("liked"), QStringLiteral("Liked"), QStringLiteral("bool")},
        {QStringLiteral("source"), QStringLiteral("Source"), QStringLiteral("source")},
        {QStringLiteral("added"), QStringLiteral("Added"), QStringLiteral("date")},
        {QStringLiteral("played"), QStringLiteral("Last played"), QStringLiteral("date")},
    };
    return fields;
}

QList<std::pair<QString, QString>> PlaylistSubsystem::GetOperators(const QString& fieldType)
{
    if (fieldType == QStringLiteral("text"))
    {
        return {{QStringLiteral("contains"), QStringLiteral("contains")},
            {QStringLiteral("notContains"), QStringLiteral("does not contain")},
            {QStringLiteral("is"), QStringLiteral("is")},
            {QStringLiteral("isNot"), QStringLiteral("is not")},
            {QStringLiteral("startsWith"), QStringLiteral("starts with")}};
    }
    if (fieldType == QStringLiteral("number"))
    {
        return {{QStringLiteral("gt"), QStringLiteral("more than")}, {QStringLiteral("lt"), QStringLiteral("less than")},
            {QStringLiteral("is"), QStringLiteral("is")}, {QStringLiteral("isNot"), QStringLiteral("is not")}};
    }
    if (fieldType == QStringLiteral("date"))
    {
        return {{QStringLiteral("inLast"), QStringLiteral("in the last, days")},
            {QStringLiteral("notInLast"), QStringLiteral("not in the last, days")}};
    }
    return {{QStringLiteral("is"), QStringLiteral("is")}, {QStringLiteral("isNot"), QStringLiteral("is not")}};
}

QList<std::pair<QString, QString>> PlaylistSubsystem::GetSortOrders()
{
    return {{QStringLiteral("added"), QStringLiteral("Recently added")},
        {QStringLiteral("played"), QStringLiteral("Recently played")},
        {QStringLiteral("plays"), QStringLiteral("Most played")},
        {QStringLiteral("title"), QStringLiteral("Title")},
        {QStringLiteral("artist"), QStringLiteral("Artist")},
        {QStringLiteral("album"), QStringLiteral("Album")},
        {QStringLiteral("year"), QStringLiteral("Year")}};
}

const Playlist* PlaylistSubsystem::Find(const QString& id) const
{
    for (const Playlist& playlist : playlists)
    {
        if (playlist.id == id)
            return &playlist;
    }
    return nullptr;
}

Playlist* PlaylistSubsystem::FindMutable(const QString& id)
{
    return const_cast<Playlist*>(Find(id));
}

void PlaylistSubsystem::Changed()
{
    saveTimer.start();
    emit playlistsChanged();
}

QString PlaylistSubsystem::CreateStatic(const QString& name, const QList<TrackId>& tracks)
{
    Playlist playlist;
    playlist.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    playlist.name = name.simplified().isEmpty() ? QStringLiteral("New playlist") : name.simplified();
    playlist.created = QDateTime::currentDateTime();
    playlists.append(playlist);

    if (!tracks.isEmpty())
        AddTracks(playlist.id, tracks);
    Changed();
    return playlist.id;
}

QString PlaylistSubsystem::CreateSmart(const QString& name, const QList<PlaylistRule>& rules, bool bMatchAll,
    const QString& sort, int limit)
{
    Playlist playlist;
    playlist.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    playlist.name = name.simplified().isEmpty() ? QStringLiteral("New smart playlist") : name.simplified();
    playlist.kind = Playlist::Kind::Smart;
    playlist.created = QDateTime::currentDateTime();
    playlist.rules = rules;
    playlist.bMatchAll = bMatchAll;
    playlist.sort = sort;
    playlist.limit = std::max(0, limit);
    playlists.append(playlist);

    Changed();
    return playlist.id;
}

void PlaylistSubsystem::Rename(const QString& id, const QString& name)
{
    Playlist* playlist = FindMutable(id);
    const QString simplified = name.simplified();
    if (!playlist || simplified.isEmpty() || playlist->name == simplified)
        return;

    playlist->name = simplified;
    Changed();
}

void PlaylistSubsystem::Delete(const QString& id)
{
    const auto removed = std::remove_if(playlists.begin(), playlists.end(), [&id](const Playlist& p) { return p.id == id; });
    if (removed == playlists.end())
        return;

    playlists.erase(removed, playlists.end());
    Changed();
}

void PlaylistSubsystem::AddTracks(const QString& id, const QList<TrackId>& tracks)
{
    Playlist* playlist = FindMutable(id);
    if (!playlist || playlist->IsSmart())
        return;

    LibrarySubsystem& library = LibrarySubsystem::Get();
    bool bChanged = false;
    for (TrackId track : tracks)
    {
        const Track* found = library.FindTrack(track);
        if (!found || playlist->tracks.contains(track))
            continue;

        if (!found->IsInLibrary())
            library.SetSaved(track, true);

        playlist->tracks.append(track);
        bChanged = true;
    }

    if (bChanged)
        Changed();
}

void PlaylistSubsystem::MoveTrack(const QString& id, int from, int to)
{
    Playlist* playlist = FindMutable(id);
    if (!playlist || playlist->IsSmart() || from == to || from < 0 || to < 0 || from >= playlist->tracks.size()
        || to >= playlist->tracks.size())
    {
        return;
    }

    playlist->tracks.move(from, to);
    Changed();
}

void PlaylistSubsystem::RemoveTracks(const QString& id, const QList<TrackId>& tracks)
{
    Playlist* playlist = FindMutable(id);
    if (!playlist)
        return;

    bool bChanged = false;
    for (TrackId track : tracks)
    {
        if (playlist->IsSmart())
        {
            if (!playlist->excluded.contains(track))
            {
                playlist->excluded.append(track);
                bChanged = true;
            }
        }
        else
        {
            bChanged |= playlist->tracks.removeAll(track) > 0;
        }
    }

    if (bChanged)
        Changed();
}

void PlaylistSubsystem::RestoreExcluded(const QString& id)
{
    Playlist* playlist = FindMutable(id);
    if (!playlist || playlist->excluded.isEmpty())
        return;

    playlist->excluded.clear();
    Changed();
}

void PlaylistSubsystem::SetSmartDefinition(const QString& id, const QList<PlaylistRule>& rules, bool bMatchAll,
    const QString& sort, int limit)
{
    Playlist* playlist = FindMutable(id);
    if (!playlist || !playlist->IsSmart())
        return;

    playlist->rules = rules;
    playlist->bMatchAll = bMatchAll;
    playlist->sort = sort;
    playlist->limit = std::max(0, limit);
    Changed();
}

void PlaylistSubsystem::ForgetTrack(TrackId track)
{
    bool bChanged = false;
    for (Playlist& playlist : playlists)
    {
        bChanged |= playlist.tracks.removeAll(track) > 0;
        bChanged |= playlist.excluded.removeAll(track) > 0;
    }

    if (bChanged)
        Changed();
}

QList<TrackId> PlaylistSubsystem::Resolve(const QString& id) const
{
    const Playlist* playlist = Find(id);
    if (!playlist)
        return {};

    if (playlist->IsSmart())
        return Evaluate(playlist->rules, playlist->bMatchAll, playlist->sort, playlist->limit, playlist->excluded);

    QList<TrackId> result;
    for (TrackId track : playlist->tracks)
    {
        if (LibrarySubsystem::Get().FindTrack(track))
            result.append(track);
    }
    return result;
}

QList<TrackId> PlaylistSubsystem::Evaluate(const QList<PlaylistRule>& rules, bool bMatchAll, const QString& sort, int limit,
    const QList<TrackId>& excluded)
{
    const QSet<TrackId> skipped(excluded.cbegin(), excluded.cend());

    QList<const Track*> matches;
    for (const Track& track : LibrarySubsystem::Get().GetTracks())
    {
        if (!track.IsInLibrary() || skipped.contains(track.id))
            continue;

        bool bMatch = rules.isEmpty() || bMatchAll;
        for (const PlaylistRule& rule : rules)
        {
            const bool bRule = Matches(track, rule);
            if (bMatchAll && !bRule)
            {
                bMatch = false;
                break;
            }
            if (!bMatchAll && bRule)
            {
                bMatch = true;
                break;
            }
        }

        if (bMatch)
            matches.append(&track);
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    const auto text = [&collator](const QString& a, const QString& b) { return collator.compare(a, b) < 0; };

    std::stable_sort(matches.begin(), matches.end(), [&](const Track* a, const Track* b) {
        if (sort == QStringLiteral("played"))
            return a->stats.lastPlayed > b->stats.lastPlayed;
        if (sort == QStringLiteral("plays"))
            return a->stats.playCount > b->stats.playCount;
        if (sort == QStringLiteral("title"))
            return text(a->tags.title, b->tags.title);
        if (sort == QStringLiteral("artist"))
            return a->tags.artist != b->tags.artist ? text(a->tags.artist, b->tags.artist) : text(a->tags.title, b->tags.title);
        if (sort == QStringLiteral("album"))
            return a->tags.album != b->tags.album ? text(a->tags.album, b->tags.album) : a->tags.trackNumber < b->tags.trackNumber;
        if (sort == QStringLiteral("year"))
            return a->tags.year > b->tags.year;
        return a->stats.dateAdded > b->stats.dateAdded;
    });

    if (limit > 0 && matches.size() > limit)
        matches.resize(limit);

    QList<TrackId> result;
    result.reserve(matches.size());
    for (const Track* track : matches)
        result.append(track->id);
    return result;
}

bool PlaylistSubsystem::Matches(const Track& track, const PlaylistRule& rule)
{
    const QString& field = rule.field;

    if (field == QStringLiteral("artist"))
        return TextMatches(track.tags.artist, rule.op, rule.value);
    if (field == QStringLiteral("title"))
        return TextMatches(track.tags.title, rule.op, rule.value);
    if (field == QStringLiteral("album"))
        return TextMatches(track.tags.album, rule.op, rule.value);
    if (field == QStringLiteral("genre"))
        return TextMatches(track.tags.genre, rule.op, rule.value);

    if (field == QStringLiteral("tag"))
    {
        const bool bNegative = rule.op == QStringLiteral("notContains") || rule.op == QStringLiteral("isNot");
        const QString positive = rule.op == QStringLiteral("notContains") ? QStringLiteral("contains")
                               : rule.op == QStringLiteral("isNot")       ? QStringLiteral("is")
                                                                          : rule.op;
        const bool bAny = std::any_of(track.user.labels.cbegin(), track.user.labels.cend(),
            [&](const QString& label) { return TextMatches(label, positive, rule.value); });
        return bNegative ? !bAny : bAny;
    }

    if (field == QStringLiteral("year"))
        return NumberMatches(track.tags.year, rule.op, rule.value);
    if (field == QStringLiteral("duration"))
        return NumberMatches(track.tags.durationMs / 60000.0, rule.op, rule.value);
    if (field == QStringLiteral("playCount"))
        return NumberMatches(track.stats.playCount, rule.op, rule.value);

    if (field == QStringLiteral("liked"))
    {
        const bool bWanted = rule.value != QStringLiteral("no");
        return (track.stats.bLiked == bWanted) == (rule.op != QStringLiteral("isNot"));
    }
    if (field == QStringLiteral("source"))
        return (SourceOf(track) == rule.value) == (rule.op != QStringLiteral("isNot"));

    if (field == QStringLiteral("added"))
        return DateMatches(track.stats.dateAdded, rule.op, rule.value);
    if (field == QStringLiteral("played"))
        return DateMatches(track.stats.lastPlayed, rule.op, rule.value);

    return false;
}

QString PlaylistSubsystem::GetStorePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/playlists.json";
}

void PlaylistSubsystem::Load()
{
    QFile file(GetStorePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value("version").toInt() != StoreVersion)
    {
        qWarning() << "[Playlists] Ignoring a store of another version:" << file.fileName();
        return;
    }

    for (const QJsonValue& value : root.value("playlists").toArray())
    {
        const QJsonObject object = value.toObject();

        Playlist playlist;
        playlist.id = object.value("id").toString();
        playlist.name = object.value("name").toString();
        playlist.kind = object.value("kind").toString() == QStringLiteral("smart") ? Playlist::Kind::Smart : Playlist::Kind::Static;
        playlist.created = QDateTime::fromMSecsSinceEpoch(object.value("created").toInteger());
        playlist.tracks = TracksFromJson(object.value("tracks").toArray());
        playlist.excluded = TracksFromJson(object.value("excluded").toArray());
        playlist.bMatchAll = object.value("matchAll").toBool(true);
        playlist.sort = object.value("sort").toString(QStringLiteral("added"));
        playlist.limit = object.value("limit").toInt();

        for (const QJsonValue& ruleValue : object.value("rules").toArray())
        {
            const QJsonObject rule = ruleValue.toObject();
            playlist.rules.append({rule.value("field").toString(), rule.value("op").toString(), rule.value("value").toString()});
        }

        if (!playlist.id.isEmpty() && !Find(playlist.id))
            playlists.append(playlist);
    }

    qDebug() << "[Playlists] Loaded" << playlists.size() << "playlists";
}

void PlaylistSubsystem::Save()
{
    QJsonArray array;
    for (const Playlist& playlist : playlists)
    {
        QJsonObject object{
            {"id", playlist.id},
            {"name", playlist.name},
            {"kind", playlist.IsSmart() ? "smart" : "static"},
            {"created", playlist.created.toMSecsSinceEpoch()},
        };

        if (playlist.IsSmart())
        {
            QJsonArray rules;
            for (const PlaylistRule& rule : playlist.rules)
                rules.append(QJsonObject{{"field", rule.field}, {"op", rule.op}, {"value", rule.value}});

            object.insert("rules", rules);
            object.insert("matchAll", playlist.bMatchAll);
            object.insert("sort", playlist.sort);
            object.insert("limit", playlist.limit);
            object.insert("excluded", ToJson(playlist.excluded));
        }
        else
        {
            object.insert("tracks", ToJson(playlist.tracks));
        }
        array.append(object);
    }

    const QString path = GetStorePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "[Playlists] Cannot save:" << file.errorString();
        return;
    }

    file.write(QJsonDocument(QJsonObject{{"version", StoreVersion}, {"playlists", array}}).toJson(QJsonDocument::Compact));
    if (!file.commit())
        qWarning() << "[Playlists] Cannot save:" << file.errorString();
}
