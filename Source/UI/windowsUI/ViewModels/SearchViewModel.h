#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include "SuggestionListModel.h"
#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;
struct OnlineEntry;

/// @brief The title bar search: suggestions while typing and the YouTube results screen. QML singleton.
///
/// A query is free text or a tag with ';'-separated parts:
///
/// - `yt:<text>` - search YouTube (also `y:`, `youtube:`)
/// - `lib:<text>` - search the library (`l:`, `library:`)
/// - `go:<page>` - open a page (`g:`)
/// - `pl:<name>` - open a playlist (`playlist:`)
/// - `s:<category>;<setting>` - open settings, e.g. `s:audio;` or `s:audio;crossfade` (`set:`, `settings:`)
///
/// Free text suggests matching pages, settings and tracks next to a YouTube search;
/// what the current screen shows is suggested first.
class SearchViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString query READ GetQuery WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(SuggestionListModel* suggestions READ GetSuggestions CONSTANT)

    Q_PROPERTY(TrackListModel* results READ GetResults CONSTANT)
    Q_PROPERTY(QString onlineQuery READ GetOnlineQuery NOTIFY onlineChanged)
    Q_PROPERTY(OnlineState onlineState READ GetOnlineState NOTIFY onlineChanged)
    Q_PROPERTY(QString onlineError READ GetOnlineError NOTIFY onlineChanged)

    /// @brief More results for the current query can be loaded (the list was scrolled to the end).
    Q_PROPERTY(bool canLoadMore READ CanLoadMore NOTIFY onlineChanged)
    Q_PROPERTY(bool loadingMore READ IsLoadingMore NOTIFY onlineChanged)

public:
    /// @brief State of the YouTube search.
    enum OnlineState
    {
        Idle,      ///< No search yet.
        Searching, ///< Waiting for the first page.
        Done,      ///< Results are shown.
        Failed     ///< See onlineError.
    };
    Q_ENUM(OnlineState)

    /// @brief The instance shared by C++ and QML.
    static SearchViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static SearchViewModel* create(QQmlEngine*, QJSEngine*);

    QString GetQuery() const { return query; }
    SuggestionListModel* GetSuggestions() { return &suggestionsModel; }
    TrackListModel* GetResults() { return &resultsModel; }
    QString GetOnlineQuery() const { return onlineQuery; }
    OnlineState GetOnlineState() const { return onlineState; }
    QString GetOnlineError() const { return onlineError; }
    bool CanLoadMore() const;
    bool IsLoadingMore() const;

    /// @brief Sets the text of the search field and rebuilds the suggestions.
    Q_INVOKABLE void setQuery(const QString& value);

    /// @brief Does what the suggestion says.
    /// @return false if the search stays open (a hint was completed).
    Q_INVOKABLE bool activate(int row);

    /// @brief Puts the suggestion's completion into the query.
    Q_INVOKABLE void complete(int row);

    /// @brief Rebuilds the suggestions, e.g. when the popup opens on another screen.
    Q_INVOKABLE void refresh();

    /// @brief Focuses the title bar search with the given text, e.g. a tag from a hint on the home screen.
    Q_INVOKABLE void focusSearch(const QString& prefill = {});

    /// @brief Opens the results screen and searches YouTube.
    Q_INVOKABLE void searchOnline(const QString& text);
    /// @brief Repeats the last YouTube search.
    Q_INVOKABLE void retryOnline();
    /// @brief Loads the next page of results.
    Q_INVOKABLE void loadMore();

signals:
    void queryChanged();
    void onlineChanged();
    /// @brief QML should focus the search field.
    void focusRequested();

private:
    SearchViewModel();

    void Rebuild();
    void OnSearchFinished(const QString& searchQuery, const QList<OnlineEntry>& entries);

    /// Adds the entries as transient online tracks after the current results, skipping ones already there.
    void AppendResults(const QList<OnlineEntry>& entries);
    void RebuildResults();
    void SetOnlineState(OnlineState state, const QString& error = {});

    QString query;
    SuggestionListModel suggestionsModel;

    QString onlineQuery;
    OnlineState onlineState = Idle;
    QString onlineError;
    QList<int> resultIds;
    TrackListModel resultsModel;
    QStringList history;
};
