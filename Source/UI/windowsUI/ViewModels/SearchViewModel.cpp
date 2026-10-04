#include "SearchViewModel.h"

#include <QJSEngine>
#include <QSet>

#include <algorithm>

#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "Navigation.h"
#include "Online/OnlineSubsystem.h"
#include "Player/PlayerSubsystem.h"
#include "Playlists/PlaylistSubsystem.h"
#include "PlayerViewModel.h"
#include "SettingsCategories.h"
#include "SettingsModel.h"

namespace
{
    constexpr int MaxSuggestions = 10;
    constexpr int MaxTrackSuggestions = 5;
    constexpr int MaxLibraryScopeTracks = 8;
    constexpr int MaxHistory = 8;
    constexpr int MaxHistorySuggestions = 3;
    constexpr int SearchResultCount = 20;

    constexpr int MaxResults = 300;

    enum class Scope
    {
        Any,
        YouTube,
        Library,
        Pages,
        Settings,
        Playlists
    };

    struct ScopeTag
    {
        Scope scope;
        QString tag;
        QStringList aliases;
    };

    const QList<ScopeTag>& GetScopeTags()
    {
        static const QList<ScopeTag> tags{
            {Scope::YouTube, QStringLiteral("yt"), {QStringLiteral("yt"), QStringLiteral("y"), QStringLiteral("youtube")}},
            {Scope::Library, QStringLiteral("lib"), {QStringLiteral("lib"), QStringLiteral("l"), QStringLiteral("library")}},
            {Scope::Pages, QStringLiteral("go"), {QStringLiteral("go"), QStringLiteral("g")}},
            {Scope::Settings, QStringLiteral("s"), {QStringLiteral("s"), QStringLiteral("set"), QStringLiteral("settings")}},
            {Scope::Playlists, QStringLiteral("pl"), {QStringLiteral("pl"), QStringLiteral("playlist"), QStringLiteral("playlists")}},
        };
        return tags;
    }

    struct ParsedQuery
    {
        Scope scope = Scope::Any;

        QStringList parts;

        QString current;
    };

    ParsedQuery Parse(const QString& text)
    {
        ParsedQuery parsed;
        QString rest = text;

        const qsizetype colon = text.indexOf(':');
        if (colon > 0)
        {
            const QString tag = text.left(colon).trimmed().toLower();
            for (const ScopeTag& scopeTag : GetScopeTags())
            {
                if (scopeTag.aliases.contains(tag))
                {
                    parsed.scope = scopeTag.scope;
                    rest = text.mid(colon + 1);
                    break;
                }
            }
        }

        if (parsed.scope != Scope::Pages && parsed.scope != Scope::Settings)
        {
            parsed.current = QString(rest).replace(';', ' ').simplified();
            return parsed;
        }

        QStringList parts = rest.split(';');
        parsed.current = parts.takeLast().simplified();
        for (const QString& part : parts)
        {
            const QString simplified = part.simplified();
            if (!simplified.isEmpty())
                parsed.parts.append(simplified);
        }
        return parsed;
    }

    int MatchScore(const QString& haystack, const QString& needle)
    {
        if (needle.isEmpty())
            return 1;

        const QString hay = haystack.toLower();
        const QString word = needle.toLower();

        if (hay == word)
            return 4;
        if (hay.startsWith(word))
            return 3;
        for (const QString& part : hay.split(' ', Qt::SkipEmptyParts))
        {
            if (part.startsWith(word))
                return 2;
        }
        return hay.contains(word) ? 1 : 0;
    }

    int BestScore(const QStringList& haystacks, const QString& needle)
    {
        int best = 0;
        for (const QString& haystack : haystacks)
            best = std::max(best, MatchScore(haystack, needle));
        return best;
    }

    QString SettingName(const QString& key)
    {
        return key.section('.', 1);
    }

    const SettingsCategory* FindCategory(const QString& text)
    {
        const SettingsCategory* best = nullptr;
        int bestScore = 1;
        for (const SettingsCategory& category : GetSettingsCategories())
        {
            const int score = BestScore({category.id, category.title}, text);
            if (score > bestScore)
            {
                best = &category;
                bestScore = score;
            }
        }
        return best;
    }

    const SettingsCategory* CategoryById(const QString& id)
    {
        for (const SettingsCategory& category : GetSettingsCategories())
        {
            if (category.id == id)
                return &category;
        }
        return nullptr;
    }

    Suggestion MakeHint(const QString& tag, const QString& title, const QString& example)
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::Hint;
        suggestion.title = title;
        suggestion.subtitle = QStringLiteral("e.g. ") + example;
        suggestion.icon = QStringLiteral("search");
        suggestion.tag = tag;
        suggestion.completion = tag;
        return suggestion;
    }

    Suggestion MakePage(const Navigation::PageInfo& page)
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::Page;
        suggestion.title = page.title;
        suggestion.subtitle = QStringLiteral("Page");
        suggestion.icon = page.icon;
        suggestion.tag = QStringLiteral("go:") + page.id;
        suggestion.completion = suggestion.tag;
        suggestion.page = page.page;
        return suggestion;
    }

    Suggestion MakeSettingsPage()
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::SettingsCategory;
        suggestion.title = QStringLiteral("Settings");
        suggestion.subtitle = QStringLiteral("Page");
        suggestion.icon = QStringLiteral("settings");
        suggestion.tag = QStringLiteral("go:settings");
        suggestion.completion = suggestion.tag;
        return suggestion;
    }

    Suggestion MakeCategory(const SettingsCategory& category)
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::SettingsCategory;
        suggestion.title = QStringLiteral("Settings › ") + category.title;
        suggestion.subtitle = QStringLiteral("Settings category");
        suggestion.icon = category.icon;
        suggestion.tag = QStringLiteral("s:%1;").arg(category.id);
        suggestion.completion = suggestion.tag;
        suggestion.category = category.id;
        return suggestion;
    }

    Suggestion MakeSetting(const SettingsModel::SettingInfo& info)
    {
        const SettingsCategory* category = CategoryById(info.category);

        Suggestion suggestion;
        suggestion.kind = Suggestion::Setting;
        suggestion.title = info.title;
        suggestion.subtitle = QStringLiteral("Settings › ") + (category ? category->title : info.category);
        suggestion.icon = category ? category->icon : QStringLiteral("settings");
        suggestion.tag = QStringLiteral("s:%1;%2").arg(info.category, SettingName(info.key));
        suggestion.completion = suggestion.tag;
        suggestion.category = info.category;
        suggestion.settingKey = info.key;
        return suggestion;
    }

    Suggestion MakeTrack(const Track& track)
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::Track;
        suggestion.title = track.tags.title;
        suggestion.subtitle = track.tags.artist.isEmpty() ? track.tags.album : track.tags.artist;
        suggestion.icon = track.IsOnline() ? QStringLiteral("globe") : QStringLiteral("library");
        suggestion.tag = QStringLiteral("lib:");
        suggestion.completion = QStringLiteral("lib:") + track.tags.title;
        suggestion.trackId = static_cast<int>(track.id.value);
        return suggestion;
    }

    Suggestion MakeYouTube(const QString& text, bool bFromHistory = false)
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::YouTube;
        suggestion.title = bFromHistory ? text : QStringLiteral("Search YouTube for “%1”").arg(text);
        suggestion.subtitle = bFromHistory ? QStringLiteral("Recent YouTube search") : QStringLiteral("Online results through yt-dlp");
        suggestion.icon = bFromHistory ? QStringLiteral("clock") : QStringLiteral("globe");
        suggestion.tag = QStringLiteral("yt:") + text;
        suggestion.completion = suggestion.tag;
        suggestion.text = text;
        return suggestion;
    }

    QList<Suggestion> MatchTracks(const QString& text, int max)
    {
        const QStringList words = text.toLower().split(' ', Qt::SkipEmptyParts);
        if (words.isEmpty())
            return {};

        QList<Suggestion> matches;
        for (const Track& track : LibrarySubsystem::Get().GetTracks())
        {
            if (!track.IsInLibrary())
                continue;

            const QString haystack = (track.tags.title + ' ' + track.tags.artist + ' ' + track.tags.album).toLower();
            const bool bAll = std::all_of(words.cbegin(), words.cend(),
                [&haystack](const QString& word) { return haystack.contains(word); });
            if (!bAll)
                continue;

            matches.append(MakeTrack(track));
            if (matches.size() >= max)
                break;
        }
        return matches;
    }

    struct Scored
    {
        int score;
        Suggestion suggestion;
    };

    void SortByScore(QList<Scored>& list)
    {
        std::stable_sort(list.begin(), list.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });
    }

    void AppendAll(QList<Suggestion>& to, const QList<Scored>& from)
    {
        for (const Scored& scored : from)
            to.append(scored.suggestion);
    }

    Suggestion MakePlaylist(const Playlist& playlist)
    {
        Suggestion suggestion;
        suggestion.kind = Suggestion::Playlist;
        suggestion.title = playlist.name;
        suggestion.subtitle = playlist.IsSmart() ? QStringLiteral("Smart playlist") : QStringLiteral("Playlist");
        suggestion.icon = playlist.IsSmart() ? QStringLiteral("funnel") : QStringLiteral("list");
        suggestion.tag = QStringLiteral("pl:") + playlist.name;
        suggestion.completion = suggestion.tag;
        suggestion.playlistId = playlist.id;
        return suggestion;
    }

    QList<Scored> MatchPlaylists(const QString& text, int bonus = 0)
    {
        QList<Scored> matches;
        for (const Playlist& playlist : PlaylistSubsystem::Get().GetPlaylists())
        {
            if (const int score = MatchScore(playlist.name, text))
                matches.append({score + bonus, MakePlaylist(playlist)});
        }
        return matches;
    }
}

SearchViewModel::SearchViewModel()
{
    OnlineSubsystem& online = OnlineSubsystem::Get();
    connect(&online, &OnlineSubsystem::searchStarted, this, [this](const QString& searchQuery) {
        onlineQuery = searchQuery;
        resultIds.clear();
        RebuildResults();
        SetOnlineState(Searching);
    });
    connect(&online, &OnlineSubsystem::searchFinished, this, &SearchViewModel::OnSearchFinished);
    connect(&online, &OnlineSubsystem::searchFailed, this, [this](const QString& searchQuery, const QString& error) {
        if (searchQuery == onlineQuery)
            SetOnlineState(Failed, error);
    });
    connect(&online, &OnlineSubsystem::moreResultsFinished, this,
        [this](const QString& searchQuery, const QList<OnlineEntry>& entries) {
            if (searchQuery != onlineQuery)
                return;
            AppendResults(entries);
            emit onlineChanged();
        });
    connect(&online, &OnlineSubsystem::moreResultsFailed, this, [this] { emit onlineChanged(); });

    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, [this] {
        RebuildResults();
        if (!query.trimmed().isEmpty())
            Rebuild();
    });

    connect(&Navigation::Get(), &Navigation::pageChanged, this, &SearchViewModel::Rebuild);
    connect(&Navigation::Get(), &Navigation::settingsCategoryChanged, this, &SearchViewModel::Rebuild);

    Rebuild();
}

SearchViewModel& SearchViewModel::Get()
{
    static SearchViewModel instance;
    return instance;
}

SearchViewModel* SearchViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

void SearchViewModel::setQuery(const QString& value)
{
    if (query == value)
        return;

    query = value;
    emit queryChanged();
    Rebuild();
}

void SearchViewModel::refresh()
{
    Rebuild();
}

void SearchViewModel::focusSearch(const QString& prefill)
{
    setQuery(prefill);
    emit focusRequested();
}

void SearchViewModel::Rebuild()
{
    const ParsedQuery parsed = Parse(query);
    const bool bOnSettings = Navigation::Get().GetPage() == Navigation::Settings;
    const QString currentCategory = Navigation::Get().GetSettingsCategory();
    const QList<SettingsModel::SettingInfo> settings = SettingsModel::Get().GetSettingInfos();

    QList<Suggestion> result;

    if (query.trimmed().isEmpty())
    {
        if (bOnSettings)
        {
            for (const SettingsModel::SettingInfo& info : settings)
            {
                if (info.category == currentCategory)
                    result.append(MakeSetting(info));
            }
        }

        for (const QString& text : history.first(std::min<qsizetype>(history.size(), MaxHistorySuggestions)))
            result.append(MakeYouTube(text, true));

        result.append(MakeHint(QStringLiteral("yt:"), QStringLiteral("Search YouTube"), QStringLiteral("yt:daft punk")));
        result.append(MakeHint(QStringLiteral("lib:"), QStringLiteral("Search your library"), QStringLiteral("lib:homework")));
        result.append(MakeHint(QStringLiteral("s:"), QStringLiteral("Open settings"), QStringLiteral("s:audio;crossfade")));
        result.append(MakeHint(QStringLiteral("go:"), QStringLiteral("Go to a page"), QStringLiteral("go:favorites")));
        result.append(MakeHint(QStringLiteral("pl:"), QStringLiteral("Open a playlist"), QStringLiteral("pl:road trip")));
    }
    else
    {
        switch (parsed.scope)
        {
            case Scope::YouTube:
            {
                if (!parsed.current.isEmpty())
                    result.append(MakeYouTube(parsed.current));
                for (const QString& text : history)
                {
                    if (text != parsed.current && MatchScore(text, parsed.current) > 0)
                        result.append(MakeYouTube(text, true));
                }
                break;
            }

            case Scope::Library:
            {
                result = MatchTracks(parsed.current, MaxLibraryScopeTracks);
                break;
            }

            case Scope::Playlists:
            {
                QList<Scored> playlists = MatchPlaylists(parsed.current);
                SortByScore(playlists);
                AppendAll(result, playlists);
                break;
            }

            case Scope::Pages:
            {
                const QString needle = parsed.parts.isEmpty() ? parsed.current : parsed.parts.first();
                QList<Scored> pages;
                for (const Navigation::PageInfo& page : Navigation::GetPages())
                {
                    if (const int score = BestScore({page.id, page.title}, needle))
                        pages.append({score, MakePage(page)});
                }
                if (const int score = MatchScore(QStringLiteral("settings"), needle))
                    pages.append({score, MakeSettingsPage()});
                SortByScore(pages);
                AppendAll(result, pages);
                break;
            }

            case Scope::Settings:
            {
                const SettingsCategory* category = parsed.parts.isEmpty() ? nullptr : FindCategory(parsed.parts.first());

                if (!category)
                {
                    const QString needle = parsed.parts.isEmpty() ? parsed.current : parsed.parts.first();
                    QList<Scored> matches;
                    for (const SettingsCategory& each : GetSettingsCategories())
                    {
                        if (const int score = BestScore({each.id, each.title}, needle))
                            matches.append({score + 1, MakeCategory(each)});
                    }
                    for (const SettingsModel::SettingInfo& info : settings)
                    {
                        if (const int score = BestScore({info.title, SettingName(info.key)}, needle))
                            matches.append({score, MakeSetting(info)});
                    }
                    SortByScore(matches);
                    AppendAll(result, matches);
                    break;
                }

                const QString needle = parsed.parts.size() >= 2 ? parsed.parts[1] : parsed.current;
                QList<Scored> matches;
                for (const SettingsModel::SettingInfo& info : settings)
                {
                    if (info.category != category->id)
                        continue;
                    if (const int score = BestScore({info.title, SettingName(info.key)}, needle))
                        matches.append({score, MakeSetting(info)});
                }
                SortByScore(matches);

                if (needle.isEmpty())
                    result.append(MakeCategory(*category));
                AppendAll(result, matches);
                if (!needle.isEmpty())
                    result.append(MakeCategory(*category));
                break;
            }

            case Scope::Any:
            {
                const QString& text = parsed.current;
                QList<Scored> navigation;

                for (const Navigation::PageInfo& page : Navigation::GetPages())
                {
                    if (const int score = BestScore({page.id, page.title}, text))
                        navigation.append({score, MakePage(page)});
                }
                if (const int score = MatchScore(QStringLiteral("settings"), text))
                    navigation.append({score, MakeSettingsPage()});
                for (const SettingsCategory& category : GetSettingsCategories())
                {
                    const int score = BestScore({category.id, category.title}, text);
                    if (score > 0)
                        navigation.append({score + (bOnSettings ? 1 : 0), MakeCategory(category)});
                }
                for (const SettingsModel::SettingInfo& info : settings)
                {
                    const int score = BestScore({info.title, SettingName(info.key)}, text);
                    if (score > 0)
                    {
                        const int contextBonus = bOnSettings ? (info.category == currentCategory ? 2 : 1) : 0;
                        navigation.append({score + contextBonus, MakeSetting(info)});
                    }
                }
                navigation.append(MatchPlaylists(text, 1));
                SortByScore(navigation);

                const bool bNavigationFirst = !navigation.isEmpty() && navigation.first().score >= 3;
                if (bNavigationFirst)
                {
                    AppendAll(result, navigation);
                    result.append(MakeYouTube(text));
                }
                else
                {
                    result.append(MakeYouTube(text));
                    AppendAll(result, navigation);
                }
                result.append(MatchTracks(text, MaxTrackSuggestions));
                break;
            }
        }
    }

    suggestionsModel.SetSuggestions(result.first(std::min<qsizetype>(result.size(), MaxSuggestions)));
}

bool SearchViewModel::activate(int row)
{
    const Suggestion* found = suggestionsModel.At(row);
    if (!found)
        return false;

    const Suggestion suggestion = *found;

    switch (suggestion.kind)
    {
        case Suggestion::Hint:
            setQuery(suggestion.completion);
            return false;

        case Suggestion::Page:
            Navigation::Get().navigate(static_cast<Navigation::Page>(suggestion.page));
            break;

        case Suggestion::SettingsCategory:
            Navigation::Get().openSettings(suggestion.category);
            break;

        case Suggestion::Setting:
            Navigation::Get().openSettings(suggestion.category, suggestion.settingKey);
            break;

        case Suggestion::Track:
            PlayerViewModel::Get().playFromLibrary(suggestion.trackId);
            break;

        case Suggestion::YouTube:
            searchOnline(suggestion.text);
            break;

        case Suggestion::Playlist:
            Navigation::Get().openPlaylist(suggestion.playlistId);
            break;
    }

    setQuery({});
    return true;
}

void SearchViewModel::complete(int row)
{
    if (const Suggestion* suggestion = suggestionsModel.At(row); suggestion && !suggestion->completion.isEmpty())
        setQuery(suggestion->completion);
}

void SearchViewModel::searchOnline(const QString& text)
{
    const QString trimmed = text.simplified();
    if (trimmed.isEmpty())
        return;

    history.removeAll(trimmed);
    history.prepend(trimmed);
    while (history.size() > MaxHistory)
        history.removeLast();

    Navigation::Get().navigate(Navigation::Search);
    OnlineSubsystem::Get().Search(trimmed, SearchResultCount);
}

void SearchViewModel::retryOnline()
{
    if (!onlineQuery.isEmpty())
        OnlineSubsystem::Get().Search(onlineQuery, SearchResultCount);
}

bool SearchViewModel::CanLoadMore() const
{
    return onlineState == Done && resultIds.size() < MaxResults && OnlineSubsystem::Get().CanSearchMore();
}

bool SearchViewModel::IsLoadingMore() const
{
    return OnlineSubsystem::Get().IsSearchingMore();
}

void SearchViewModel::loadMore()
{
    if (!CanLoadMore() || IsLoadingMore())
        return;

    OnlineSubsystem::Get().SearchMore();
    emit onlineChanged();
}

void SearchViewModel::OnSearchFinished(const QString& searchQuery, const QList<OnlineEntry>& entries)
{
    if (searchQuery != onlineQuery)
        return;

    LibrarySubsystem::Get().RemoveTransientTracks({PlayerSubsystem::Get().GetCurrentTrack()});

    resultIds.clear();
    AppendResults(entries);
    SetOnlineState(Done);
}

void SearchViewModel::AppendResults(const QList<OnlineEntry>& entries)
{
    LibrarySubsystem& library = LibrarySubsystem::Get();
    for (const OnlineEntry& entry : entries)
    {
        TagsComponent tags;
        OnlineSubsystem::GuessArtistAndTitle(entry, tags.artist, tags.title);
        tags.album = entry.channel;
        tags.durationMs = entry.durationMs;

        const TrackId id = library.AddOnlineTrack(entry.pageUrl, tags, entry.thumbnailUrl);
        if (id.IsValid() && !resultIds.contains(static_cast<int>(id.value)) && resultIds.size() < MaxResults)
            resultIds.append(static_cast<int>(id.value));
    }

    RebuildResults();
}

void SearchViewModel::RebuildResults()
{
    QList<TrackData> data;
    for (int id : resultIds)
    {
        if (const Track* track = LibrarySubsystem::Get().FindTrack(TrackId{static_cast<quint32>(id)}))
            data.append(LibraryViewModel::ToTrackData(*track));
    }
    resultsModel.SetTracks(data);
}

void SearchViewModel::SetOnlineState(OnlineState state, const QString& error)
{
    onlineState = state;
    onlineError = error;
    emit onlineChanged();
}
