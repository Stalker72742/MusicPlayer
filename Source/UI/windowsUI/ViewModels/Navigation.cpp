#include "Navigation.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

#include "Catalog/CatalogSubsystem.h"
#include "Playlists/PlaylistSubsystem.h"
#include "SettingsCategories.h"

namespace
{
    constexpr int MaxHistory = 50;
    constexpr int SaveDelayMs = 1000;
    constexpr int SnapshotVersion = 1;

    template <typename T>
    void Trim(QList<T>& stack)
    {
        while (stack.size() > MaxHistory)
            stack.removeFirst();
    }
}

bool Navigation::Location::IsSamePlace(const Location& other) const
{
    return page == other.page && settingsCategory == other.settingsCategory && playlistId == other.playlistId
        && onlineUrl == other.onlineUrl && searchQuery == other.searchQuery && collectionKind == other.collectionKind
        && collectionKey == other.collectionKey;
}

Navigation::Navigation()
{
    current.settingsCategory = GetSettingsCategories().first().id;

    saveTimer.setSingleShot(true);
    saveTimer.setInterval(SaveDelayMs);
    connect(&saveTimer, &QTimer::timeout, this, &Navigation::SaveSnapshot);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        emit saveStateRequested();
        saveTimer.stop();
        SaveSnapshot();
    });

    LoadSnapshot();
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
    Location location = current;
    location.page = target;
    GoTo(location, Move::New);
}

void Navigation::openPlaylist(const QString& id)
{
    if (id.isEmpty())
        return;

    Location location = current;
    location.page = Playlist;
    location.playlistId = id;
    GoTo(location, Move::New);
}

void Navigation::openOnline(const QString& url)
{
    if (url.isEmpty())
        return;

    Location location = current;
    location.page = OnlinePage;
    location.onlineUrl = url;
    GoTo(location, Move::New);
}

void Navigation::openSearch(const QString& query)
{
    if (query.isEmpty())
        return;

    Location location = current;
    location.page = Search;
    location.searchQuery = query;
    GoTo(location, Move::New);
}

void Navigation::openCollection(const QString& kind, const QString& key)
{
    if (!ParseCatalogKind(kind) || key.isEmpty())
        return;

    Location location = current;
    location.page = Collection;
    location.collectionKind = kind;
    location.collectionKey = key;
    GoTo(location, Move::New);
}

void Navigation::openSettings(const QString& category, const QString& settingKey)
{
    if (!category.isEmpty() && !IsSettingsCategory(category))
    {
        qWarning() << "[Navigation] Unknown settings category:" << category;
        return;
    }

    Location location = current;
    location.page = Settings;
    if (!category.isEmpty())
        location.settingsCategory = category;
    GoTo(location, Move::New);

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
    while (!backStack.isEmpty())
    {
        const Location previous = backStack.takeLast();
        if (IsAvailable(previous) && !previous.IsSamePlace(current))
        {
            GoTo(previous, Move::Back);
            return;
        }
    }
    emit historyChanged();
}

void Navigation::goForward()
{
    while (!forwardStack.isEmpty())
    {
        const Location next = forwardStack.takeLast();
        if (IsAvailable(next) && !next.IsSamePlace(current))
        {
            GoTo(next, Move::Forward);
            return;
        }
    }
    emit historyChanged();
}

void Navigation::storeState(const QVariantMap& state)
{
    current.state = state;
}

bool Navigation::IsAvailable(const Location& location)
{
    if (location.page == Playlist)
        return PlaylistSubsystem::Get().Find(location.playlistId) != nullptr;
    if (location.page == Settings)
        return IsSettingsCategory(location.settingsCategory);
    if (location.page == Collection)
    {
        const std::optional<CatalogKind> kind = ParseCatalogKind(location.collectionKind);
        return kind && CatalogSubsystem::Get().Find(*kind, location.collectionKey) != nullptr;
    }
    return true;
}

void Navigation::GoTo(const Location& location, Move move)
{
    if (location.IsSamePlace(current))
        return;

    emit saveStateRequested();

    switch (move)
    {
    case Move::New:
        backStack.append(current);
        forwardStack.clear();
        break;
    case Move::Back:
        forwardStack.append(current);
        break;
    case Move::Forward:
        backStack.append(current);
        break;
    }
    Trim(backStack);
    Trim(forwardStack);

    const Location previous = std::exchange(current, location);
    if (move == Move::New)
        current.state.clear();

    emit historyChanged();
    emit locationChanged();

    if (current.settingsCategory != previous.settingsCategory)
        emit settingsCategoryChanged();
    if (current.playlistId != previous.playlistId)
        emit playlistIdChanged();
    if (current.onlineUrl != previous.onlineUrl)
        emit onlineUrlChanged();
    if (current.searchQuery != previous.searchQuery)
        emit searchQueryChanged();
    if (current.collectionKind != previous.collectionKind || current.collectionKey != previous.collectionKey)
        emit collectionChanged();
    if (current.page != previous.page)
        emit pageChanged();

    saveTimer.start();
}

QJsonObject Navigation::ToJson(const Location& location)
{
    QJsonObject object{
        {QStringLiteral("page"), QString::fromLatin1(QMetaEnum::fromType<Page>().valueToKey(location.page))},
    };
    if (!location.settingsCategory.isEmpty())
        object.insert(QStringLiteral("settingsCategory"), location.settingsCategory);
    if (!location.playlistId.isEmpty())
        object.insert(QStringLiteral("playlistId"), location.playlistId);
    if (!location.onlineUrl.isEmpty())
        object.insert(QStringLiteral("onlineUrl"), location.onlineUrl);
    if (!location.searchQuery.isEmpty())
        object.insert(QStringLiteral("searchQuery"), location.searchQuery);
    if (!location.collectionKind.isEmpty())
        object.insert(QStringLiteral("collectionKind"), location.collectionKind);
    if (!location.collectionKey.isEmpty())
        object.insert(QStringLiteral("collectionKey"), location.collectionKey);
    if (!location.state.isEmpty())
        object.insert(QStringLiteral("state"), QJsonObject::fromVariantMap(location.state));
    return object;
}

bool Navigation::FromJson(const QJsonObject& object, Location& location)
{
    bool bOk = false;
    const int page = QMetaEnum::fromType<Page>().keyToValue(object.value(QStringLiteral("page")).toString().toLatin1(), &bOk);
    if (!bOk)
        return false;

    location.page = static_cast<Page>(page);
    location.settingsCategory = object.value(QStringLiteral("settingsCategory")).toString();
    location.playlistId = object.value(QStringLiteral("playlistId")).toString();
    location.onlineUrl = object.value(QStringLiteral("onlineUrl")).toString();
    location.searchQuery = object.value(QStringLiteral("searchQuery")).toString();
    location.collectionKind = object.value(QStringLiteral("collectionKind")).toString();
    location.collectionKey = object.value(QStringLiteral("collectionKey")).toString();
    location.state = object.value(QStringLiteral("state")).toObject().toVariantMap();
    if (!IsSettingsCategory(location.settingsCategory))
        location.settingsCategory = GetSettingsCategories().first().id;
    return true;
}

QString Navigation::GetSnapshotPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/navigation.json");
}

void Navigation::LoadSnapshot()
{
    QFile file(GetSnapshotPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("version")).toInt() != SnapshotVersion)
        return;

    const auto readStack = [](const QJsonArray& array) {
        QList<Location> stack;
        for (const QJsonValue& value : array)
        {
            Location location;
            if (FromJson(value.toObject(), location))
                stack.append(location);
        }
        Trim(stack);
        return stack;
    };

    Location saved;
    if (!FromJson(root.value(QStringLiteral("current")).toObject(), saved))
        return;

    backStack = readStack(root.value(QStringLiteral("back")).toArray());
    forwardStack = readStack(root.value(QStringLiteral("forward")).toArray());

    if (IsAvailable(saved))
    {
        current = saved;
    }
    else
    {
        current.settingsCategory = saved.settingsCategory;
        forwardStack.clear();
    }
}

void Navigation::SaveSnapshot() const
{
    const QString path = GetSnapshotPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray back;
    for (const Location& location : backStack)
        back.append(ToJson(location));
    QJsonArray forward;
    for (const Location& location : forwardStack)
        forward.append(ToJson(location));

    const QJsonObject root{
        {QStringLiteral("version"), SnapshotVersion},
        {QStringLiteral("current"), ToJson(current)},
        {QStringLiteral("back"), back},
        {QStringLiteral("forward"), forward},
    };

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "[Navigation] Cannot write" << path;
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}
