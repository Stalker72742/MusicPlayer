#include "PlaylistsViewModel.h"

#include <QJSEngine>

#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "Navigation.h"
#include "Playlists/PlaylistSubsystem.h"

namespace
{
    QColor PlaylistTint(const QString& id)
    {
        const int hue = static_cast<int>(qHash(id) % 360);
        return QColor::fromHsl(hue, 60, 38);
    }

    QVariantList Pairs(const QList<std::pair<QString, QString>>& pairs)
    {
        QVariantList result;
        for (const auto& [id, title] : pairs)
            result.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), title}});
        return result;
    }

    QList<std::pair<QString, QString>> ValueOptions(const QString& type)
    {
        if (type == QStringLiteral("bool"))
            return {{QStringLiteral("yes"), QStringLiteral("yes")}, {QStringLiteral("no"), QStringLiteral("no")}};
        if (type == QStringLiteral("source"))
        {
            return {{QStringLiteral("local"), QStringLiteral("a local file")},
                {QStringLiteral("youtube"), QStringLiteral("streamed from YouTube")},
                {QStringLiteral("downloaded"), QStringLiteral("downloaded from YouTube")}};
        }
        return {};
    }

    QString TitleOf(const QList<std::pair<QString, QString>>& pairs, const QString& id)
    {
        for (const auto& [pairId, title] : pairs)
        {
            if (pairId == id)
                return title;
        }
        return id;
    }
}

PlaylistsViewModel::PlaylistsViewModel()
{
    connect(&PlaylistSubsystem::Get(), &PlaylistSubsystem::playlistsChanged, this, &PlaylistsViewModel::Rebuild);
    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, &PlaylistsViewModel::Rebuild);
    connect(&Navigation::Get(), &Navigation::playlistIdChanged, this, &PlaylistsViewModel::Rebuild);

    Rebuild();
}

PlaylistsViewModel& PlaylistsViewModel::Get()
{
    static PlaylistsViewModel instance;
    return instance;
}

PlaylistsViewModel* PlaylistsViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

void PlaylistsViewModel::Rebuild()
{
    PlaylistSubsystem& subsystem = PlaylistSubsystem::Get();
    LibrarySubsystem& library = LibrarySubsystem::Get();

    QList<PlaylistData> list;
    for (const Playlist& playlist : subsystem.GetPlaylists())
    {
        const QList<TrackId> tracks = subsystem.Resolve(playlist.id);

        PlaylistData data;
        data.id = playlist.id;
        data.name = playlist.name;
        data.bSmart = playlist.IsSmart();
        data.trackCount = static_cast<int>(tracks.size());
        data.artTint = PlaylistTint(playlist.id);
        for (TrackId id : tracks)
        {
            const Track* track = library.FindTrack(id);
            if (track && !track->stream.thumbnailUrl.isEmpty())
            {
                data.artUrl = track->stream.thumbnailUrl;
                break;
            }
        }
        list.append(data);
    }
    playlistsModel.SetPlaylists(list);

    const QString openId = Navigation::Get().GetPlaylistId();
    const Playlist* open = subsystem.Find(openId);

    QList<TrackData> tracks;
    currentDownloadable = 0;
    if (open)
    {
        for (TrackId id : subsystem.Resolve(open->id))
        {
            if (const Track* track = library.FindTrack(id))
            {
                tracks.append(LibraryViewModel::ToTrackData(*track));
                if (track->IsOnline() && !track->HasLocalFile())
                    ++currentDownloadable;
            }
        }
    }
    currentTracks.SetTracks(tracks);

    currentId = open ? open->id : QString();
    currentName = open ? open->name : QString();
    bCurrentSmart = open && open->IsSmart();
    currentSummary = open && open->IsSmart() ? Summarize(open->rules, open->bMatchAll) : QString();
    currentExcluded = open && open->IsSmart() ? static_cast<int>(open->excluded.size()) : 0;
    emit currentChanged();
}

QString PlaylistsViewModel::createPlaylist(const QString& name, const QVariantList& trackIds)
{
    QList<TrackId> tracks;
    for (const QVariant& id : trackIds)
        tracks.append(TrackId{static_cast<quint32>(id.toInt())});
    return PlaylistSubsystem::Get().CreateStatic(name, tracks);
}

QString PlaylistsViewModel::createSmartPlaylist(const QString& name, const QVariantMap& definition)
{
    return PlaylistSubsystem::Get().CreateSmart(name, RulesFromVariant(definition),
        definition.value(QStringLiteral("matchAll"), true).toBool(),
        definition.value(QStringLiteral("sort"), QStringLiteral("added")).toString(),
        definition.value(QStringLiteral("limit")).toInt());
}

void PlaylistsViewModel::rename(const QString& id, const QString& name)
{
    PlaylistSubsystem::Get().Rename(id, name);
}

void PlaylistsViewModel::remove(const QString& id)
{
    if (Navigation::Get().GetPlaylistId() == id && Navigation::Get().GetPage() == Navigation::Playlist)
        Navigation::Get().navigate(Navigation::Playlists);
    PlaylistSubsystem::Get().Delete(id);
}

void PlaylistsViewModel::open(const QString& id)
{
    Navigation::Get().openPlaylist(id);
}

void PlaylistsViewModel::addTrack(const QString& id, int trackId)
{
    PlaylistSubsystem::Get().AddTracks(id, {TrackId{static_cast<quint32>(trackId)}});
}

void PlaylistsViewModel::removeTrack(const QString& id, int trackId)
{
    PlaylistSubsystem::Get().RemoveTracks(id, {TrackId{static_cast<quint32>(trackId)}});
}

void PlaylistsViewModel::moveTrack(const QString& id, int from, int to)
{
    PlaylistSubsystem::Get().MoveTrack(id, from, to);
}

void PlaylistsViewModel::restoreExcluded(const QString& id)
{
    PlaylistSubsystem::Get().RestoreExcluded(id);
}

QVariantList PlaylistsViewModel::staticPlaylistsFor(int trackId) const
{
    const TrackId id{static_cast<quint32>(trackId)};

    QVariantList result;
    for (const Playlist& playlist : PlaylistSubsystem::Get().GetPlaylists())
    {
        if (playlist.IsSmart())
            continue;

        result.append(QVariantMap{
            {QStringLiteral("id"), playlist.id},
            {QStringLiteral("name"), playlist.name},
            {QStringLiteral("contains"), playlist.tracks.contains(id)},
        });
    }
    return result;
}

QString PlaylistsViewModel::nameOf(const QString& id) const
{
    const Playlist* playlist = PlaylistSubsystem::Get().Find(id);
    return playlist ? playlist->name : QString();
}

QVariantMap PlaylistsViewModel::definition(const QString& id) const
{
    const Playlist* playlist = PlaylistSubsystem::Get().Find(id);
    if (!playlist)
        return {};

    QVariantList rules;
    for (const PlaylistRule& rule : playlist->rules)
    {
        rules.append(QVariantMap{
            {QStringLiteral("field"), rule.field},
            {QStringLiteral("op"), rule.op},
            {QStringLiteral("value"), rule.value},
        });
    }

    return {
        {QStringLiteral("rules"), rules},
        {QStringLiteral("matchAll"), playlist->bMatchAll},
        {QStringLiteral("sort"), playlist->sort},
        {QStringLiteral("limit"), playlist->limit},
    };
}

void PlaylistsViewModel::setDefinition(const QString& id, const QVariantMap& definition)
{
    PlaylistSubsystem::Get().SetSmartDefinition(id, RulesFromVariant(definition),
        definition.value(QStringLiteral("matchAll"), true).toBool(),
        definition.value(QStringLiteral("sort"), QStringLiteral("added")).toString(),
        definition.value(QStringLiteral("limit")).toInt());
}

int PlaylistsViewModel::previewCount(const QVariantMap& definition) const
{
    return static_cast<int>(PlaylistSubsystem::Evaluate(RulesFromVariant(definition),
        definition.value(QStringLiteral("matchAll"), true).toBool(),
        definition.value(QStringLiteral("sort"), QStringLiteral("added")).toString(),
        definition.value(QStringLiteral("limit")).toInt()).size());
}

QList<PlaylistRule> PlaylistsViewModel::RulesFromVariant(const QVariantMap& definition)
{
    QList<PlaylistRule> rules;
    for (const QVariant& value : definition.value(QStringLiteral("rules")).toList())
    {
        const QVariantMap rule = value.toMap();
        rules.append({rule.value(QStringLiteral("field")).toString(), rule.value(QStringLiteral("op")).toString(),
            rule.value(QStringLiteral("value")).toString()});
    }
    return rules;
}

QVariantList PlaylistsViewModel::ruleFields() const
{
    QVariantList result;
    for (const PlaylistSubsystem::RuleField& field : PlaylistSubsystem::GetRuleFields())
    {
        result.append(QVariantMap{
            {QStringLiteral("id"), field.id},
            {QStringLiteral("title"), field.title},
            {QStringLiteral("type"), field.type},
        });
    }
    return result;
}

QVariantList PlaylistsViewModel::operators(const QString& type) const
{
    return Pairs(PlaylistSubsystem::GetOperators(type));
}

QVariantList PlaylistsViewModel::sortOrders() const
{
    return Pairs(PlaylistSubsystem::GetSortOrders());
}

QVariantList PlaylistsViewModel::valueOptions(const QString& type) const
{
    return Pairs(ValueOptions(type));
}

QString PlaylistsViewModel::fieldType(const QString& field) const
{
    for (const PlaylistSubsystem::RuleField& each : PlaylistSubsystem::GetRuleFields())
    {
        if (each.id == field)
            return each.type;
    }
    return QStringLiteral("text");
}

QString PlaylistsViewModel::Summarize(const QList<PlaylistRule>& rules, bool bMatchAll)
{
    if (rules.isEmpty())
        return QStringLiteral("Every track in the library");

    QStringList parts;
    for (const PlaylistRule& rule : rules)
    {
        QString fieldTitle = rule.field;
        QString type = QStringLiteral("text");
        for (const PlaylistSubsystem::RuleField& field : PlaylistSubsystem::GetRuleFields())
        {
            if (field.id == rule.field)
            {
                fieldTitle = field.title;
                type = field.type;
            }
        }

        const QString op = TitleOf(PlaylistSubsystem::GetOperators(type), rule.op);
        QString value = rule.value;
        if (type == QStringLiteral("source") || type == QStringLiteral("bool"))
            value = TitleOf(ValueOptions(type), rule.value);
        else if (type == QStringLiteral("text"))
            value = QStringLiteral("“%1”").arg(rule.value);

        parts.append(QStringLiteral("%1 %2 %3").arg(fieldTitle, op, value));
    }
    return parts.join(bMatchAll ? QStringLiteral(" and ") : QStringLiteral(" or "));
}
