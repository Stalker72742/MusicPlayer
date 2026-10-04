#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// @brief Which screen the main window shows, with a back history. QML singleton.
///
/// Settings additionally have a category
/// and, when opened from search, a setting to point at; the playlist screen has a playlist.
class Navigation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Page page READ GetPage NOTIFY pageChanged)
    Q_PROPERTY(QString settingsCategory READ GetSettingsCategory NOTIFY settingsCategoryChanged)
    Q_PROPERTY(QString highlightedSetting READ GetHighlightedSetting NOTIFY highlightedSettingChanged)
    Q_PROPERTY(QString playlistId READ GetPlaylistId NOTIFY playlistIdChanged)
    Q_PROPERTY(bool canGoBack READ CanGoBack NOTIFY historyChanged)

public:
    /// @brief Screens of the main window. Order matches the screens in the main window's StackLayout.
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
        Settings,
        About,
        Updates
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

    Page GetPage() const { return page; }
    QString GetSettingsCategory() const { return settingsCategory; }
    QString GetHighlightedSetting() const { return highlightedSetting; }
    QString GetPlaylistId() const { return playlistId; }
    bool CanGoBack() const { return !history.isEmpty(); }

    /// @brief Opens a page, recording the current one in the history.
    Q_INVOKABLE void navigate(Navigation::Page target);

    /// @brief Opens the settings screen on a category; an empty category keeps the last one.
    /// A setting key points the screen at that setting.
    Q_INVOKABLE void openSettings(const QString& category = {}, const QString& settingKey = {});

    /// @brief Opens the screen of one playlist.
    Q_INVOKABLE void openPlaylist(const QString& id);

    /// @brief Returns to the previous location, if any.
    Q_INVOKABLE void goBack();

signals:
    void pageChanged();
    void settingsCategoryChanged();
    void highlightedSettingChanged();
    void playlistIdChanged();
    void historyChanged();

private:
    Navigation();

    struct Location
    {
        Page page;
        QString settingsCategory;
        QString playlistId;

        bool operator==(const Location&) const = default;
    };

    void GoTo(const Location& location, bool bRecordHistory);

    Page page = Home;
    QString settingsCategory;
    QString highlightedSetting;
    QString playlistId;
    QList<Location> history;
};
