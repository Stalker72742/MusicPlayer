#include "Navigation.h"

#include <QDebug>
#include <QJSEngine>

#include "SettingsCategories.h"

namespace
{
    constexpr int MaxHistory = 50;
}

Navigation::Navigation()
    : settingsCategory(GetSettingsCategories().first().id)
{
}

const QList<Navigation::PageInfo>& Navigation::GetPages()
{
    static const QList<PageInfo> pages{
        {Home, QStringLiteral("home"), QStringLiteral("Home"), QStringLiteral("home")},
        {Tracks, QStringLiteral("tracks"), QStringLiteral("All Tracks"), QStringLiteral("library")},
        {Playlists, QStringLiteral("playlists"), QStringLiteral("Playlists"), QStringLiteral("list")},
        {Favorites, QStringLiteral("favorites"), QStringLiteral("Favorites"), QStringLiteral("heart")},
        {Recent, QStringLiteral("recent"), QStringLiteral("Recent"), QStringLiteral("clock")},
        {Downloads, QStringLiteral("downloads"), QStringLiteral("Downloads"), QStringLiteral("download")},
        {Search, QStringLiteral("youtube"), QStringLiteral("YouTube results"), QStringLiteral("globe")},
        {About, QStringLiteral("about"), QStringLiteral("About"), QStringLiteral("info")},
        {Updates, QStringLiteral("updates"), QStringLiteral("Updates"), QStringLiteral("download")},
    };
    return pages;
}

Navigation& Navigation::Get()
{
    static Navigation instance;
    return instance;
}

Navigation* Navigation::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

void Navigation::navigate(Page target)
{
    GoTo({target, settingsCategory, playlistId}, true);
}

void Navigation::openPlaylist(const QString& id)
{
    if (!id.isEmpty())
        GoTo({Playlist, settingsCategory, id}, true);
}

void Navigation::openSettings(const QString& category, const QString& settingKey)
{
    if (!category.isEmpty() && !IsSettingsCategory(category))
    {
        qWarning() << "[Navigation] Unknown settings category:" << category;
        return;
    }

    GoTo({Settings, category.isEmpty() ? settingsCategory : category, playlistId}, true);

    highlightedSetting.clear();
    emit highlightedSettingChanged();
    if (!settingKey.isEmpty())
    {
        highlightedSetting = settingKey;
        emit highlightedSettingChanged();
    }
}

void Navigation::goBack()
{
    if (history.isEmpty())
        return;

    const Location previous = history.takeLast();
    emit historyChanged();
    GoTo(previous, false);
}

void Navigation::GoTo(const Location& location, bool bRecordHistory)
{
    const Location current{page, settingsCategory, playlistId};
    if (location == current)
        return;

    if (bRecordHistory)
    {
        history.append(current);
        if (history.size() > MaxHistory)
            history.removeFirst();
        emit historyChanged();
    }

    const bool bPageChanged = location.page != page;
    const bool bCategoryChanged = location.settingsCategory != settingsCategory;
    const bool bPlaylistChanged = location.playlistId != playlistId;

    page = location.page;
    settingsCategory = location.settingsCategory;
    playlistId = location.playlistId;

    if (bCategoryChanged)
        emit settingsCategoryChanged();
    if (bPlaylistChanged)
        emit playlistIdChanged();
    if (bPageChanged)
        emit pageChanged();
}
