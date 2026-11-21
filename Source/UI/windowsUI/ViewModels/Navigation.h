#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// @brief Which screen the main window shows, with a back and forward history like a browser tab. QML singleton.
///
/// A location is a page plus what picks its content: the settings category, the playlist, the YouTube link,
/// the search query. Going somewhere new from the middle of the history drops what was ahead.
///
/// The main window keeps only the current screen alive. Before leaving, saveStateRequested() asks it for the
/// screen's state (scroll position, tab...), which storeState() keeps with the location; a screen created for a
/// location reads it back from pageState.
///
/// The whole history, with the states, is a snapshot in `<cache>/navigation.json`, written a moment after every
/// change and on quit; the app starts where it was left.
class Navigation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Page page READ GetPage NOTIFY pageChanged)
    Q_PROPERTY(QString settingsCategory READ GetSettingsCategory NOTIFY settingsCategoryChanged)
    Q_PROPERTY(QString highlightedSetting READ GetHighlightedSetting NOTIFY highlightedSettingChanged)
    Q_PROPERTY(QString playlistId READ GetPlaylistId NOTIFY playlistIdChanged)
    Q_PROPERTY(QString onlineUrl READ GetOnlineUrl NOTIFY onlineUrlChanged)
    Q_PROPERTY(QString searchQuery READ GetSearchQuery NOTIFY searchQueryChanged)
    /// @brief Page Collection: `"artist"`, `"album"` or `"genre"` and the CatalogEntry key.
    Q_PROPERTY(QString collectionKind READ GetCollectionKind NOTIFY collectionChanged)
    Q_PROPERTY(QString collectionKey READ GetCollectionKey NOTIFY collectionChanged)
    Q_PROPERTY(QVariantMap pageState READ GetPageState NOTIFY locationChanged)
    Q_PROPERTY(bool canGoBack READ CanGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ CanGoForward NOTIFY historyChanged)

public:
    /// @brief Screens of the main window. Order matches the screens in the main window.
    enum Page
    {
        Home,
        Tracks,
        Playlists,
        Playlist,
        Favorites,
        Recent,
        Downloads,
        Search,
        OnlinePage,
        Settings,
        About,
        Updates,
        Collection ///< An artist, album or genre of the library.
    };
    Q_ENUM(Page)

    /// @brief A page that can be opened by name (search, "go:" tags); Settings is opened by category instead.
    struct PageInfo
    {
        Page page;
        QString id;
        QString title;
        QString icon;
    };

    /// @brief Every page that can be opened by name.
    static const QList<PageInfo>& GetPages();

    /// @brief The instance shared by C++ and QML.
    static Navigation& Get();
    /// @brief QML singleton factory; returns Get().
    static Navigation* create(QQmlEngine*, QJSEngine*);

    Page GetPage() const { return current.page; }
    QString GetSettingsCategory() const { return current.settingsCategory; }
    QString GetHighlightedSetting() const { return highlightedSetting; }
    QString GetPlaylistId() const { return current.playlistId; }
    QString GetOnlineUrl() const { return current.onlineUrl; }
    /// @brief The YouTube search the Search page shows.
    QString GetSearchQuery() const { return current.searchQuery; }
    QString GetCollectionKind() const { return current.collectionKind; }
    QString GetCollectionKey() const { return current.collectionKey; }
    /// @brief What the screen of the current location saved when it was left last; empty for a new location.
    QVariantMap GetPageState() const { return current.state; }
    bool CanGoBack() const { return !backStack.isEmpty(); }
    bool CanGoForward() const { return !forwardStack.isEmpty(); }

    /// @brief Opens a page, recording the current one in the history.
    Q_INVOKABLE void navigate(Navigation::Page target);

    /// @brief Opens the settings screen on a category; an empty category keeps the last one.
    /// A setting key points the screen at that setting.
    Q_INVOKABLE void openSettings(const QString& category = {}, const QString& settingKey = {});

    /// @brief Opens the screen of one playlist.
    Q_INVOKABLE void openPlaylist(const QString& id);

    /// @brief Opens the screen of a YouTube video, playlist or channel by its canonical URL.
    Q_INVOKABLE void openOnline(const QString& url);

    /// @brief Opens the YouTube results of a query; SearchViewModel runs the search.
    Q_INVOKABLE void openSearch(const QString& query);

    /// @brief Opens an artist, album or genre (CatalogViewModel shows it).
    Q_INVOKABLE void openCollection(const QString& kind, const QString& key);

    /// @brief Returns to the previous location, if any. Deleted playlists are skipped.
    Q_INVOKABLE void goBack();
    /// @brief Goes to the location left by goBack(), if any.
    Q_INVOKABLE void goForward();

    /// @brief Keeps the state of the current screen with the current location; called on saveStateRequested().
    Q_INVOKABLE void storeState(const QVariantMap& state);

signals:
    void pageChanged();
    void settingsCategoryChanged();
    void highlightedSettingChanged();
    void playlistIdChanged();
    void onlineUrlChanged();
    void searchQueryChanged();
    void collectionChanged();
    void historyChanged();
    /// @brief The current screen is about to be left (or the app to quit): store its state with storeState().
    void saveStateRequested();
    /// @brief The location changed: the main window creates the screen for it. Comes before the property signals.
    void locationChanged();

private:
    Navigation();

    struct Location
    {
        Page page = Home;
        QString settingsCategory;
        QString playlistId;
        QString onlineUrl;
        QString searchQuery;
        QString collectionKind;
        QString collectionKey;
        /// Saved by the screen; not part of where the location is.
        QVariantMap state;

        bool IsSamePlace(const Location& other) const;
    };

    enum class Move
    {
        New,     ///< Drops the forward history.
        Back,
        Forward
    };

    void GoTo(const Location& location, Move move);
    /// Whether a location from the history can still be shown.
    static bool IsAvailable(const Location& location);

    static QJsonObject ToJson(const Location& location);
    static bool FromJson(const QJsonObject& object, Location& location);
    static QString GetSnapshotPath();
    void LoadSnapshot();
    void SaveSnapshot() const;

    Location current;
    QString highlightedSetting;
    QList<Location> backStack;
    QList<Location> forwardStack;
    QTimer saveTimer;
};
