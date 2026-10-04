//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>

#include "Online/OnlineTypes.h"

class OnlineBackend;

/// @brief Online music: search and the direct audio stream of a page, through interchangeable backends.
///
/// Backends (`private/subsystems/Online`): "innertube" (search), "browser" (streams with the browser extension's
/// token) and "ytdlp" (both, the fallback). Each operation goes to the user's preferred backend
/// (AppConfigs::SettingsKeys::OnlineSearchSource / OnlineStreamSource) and then to the others:
/// an unavailable backend is skipped at once, one that failed or got blocked rests for a while,
/// and "not found" ends the attempt. Nothing is downloaded; streams are resolved again when they expire.
class OnlineSubsystem : public Subsystem<OnlineSubsystem>
{
    Q_OBJECT
    friend class Subsystem<OnlineSubsystem>;

public:
    /// @brief Starts a search, replacing a running one.
    ///
    /// Asynchronous: the outcome arrives in searchFinished() or searchFailed().
    /// @param query Search text.
    /// @param count Number of results wanted.
    void Search(const QString& query, int count = 20);
    /// @brief Cancels the running search and its paging; no signal follows.
    void CancelSearch();
    bool IsSearching() const { return searchGeneration != 0; }

    /// @brief Whether the last search has another page.
    bool CanSearchMore() const;
    /// @brief Loads the next page of the last search.
    ///
    /// Asks the backend that found the first page (pages of different backends do not continue each other).
    /// The outcome arrives in moreResultsFinished() or moreResultsFailed().
    void SearchMore();
    bool IsSearchingMore() const { return moreGeneration != 0; }

    /// @brief Result of ResolveStream(): `stream` is empty on failure, then `error` says why.
    using StreamCallback = std::function<void(const QUrl& stream, const QString& error)>;

    /// @brief Finds the direct audio stream of a page.
    ///
    /// Requests for the same page share one attempt; a resolved stream is reused until shortly before it expires.
    /// @param pageUrl Page of an online track.
    /// @param context The callback is dropped if this object is destroyed first.
    /// @param callback Called once, on the main thread.
    void ResolveStream(const QString& pageUrl, QObject* context, StreamCallback callback);

    /// @brief Reports a stream that turned out unplayable (expired early, refused).
    ///
    /// Drops the cached stream and rests the backend that resolved it for a while, so the next attempt goes elsewhere.
    void ReportStreamFailed(const QString& pageUrl);

    /// @brief Tags for an online track: "Artist - Title" titles are split, otherwise the channel is the artist.
    /// @param entry A search result.
    /// @param[out] artist Guessed artist.
    /// @param[out] title Guessed title.
    static void GuessArtistAndTitle(const OnlineEntry& entry, QString& artist, QString& title);

    /// @brief A backend as shown in the settings.
    struct BackendInfo
    {
        QString id;          ///< Config id ("innertube", "ytdlp", "browser").
        QString name;        ///< Name for the UI.
        bool bSearch = false; ///< Can search.
        bool bStream = false; ///< Can resolve streams.

        /// @brief Empty when usable; otherwise why not, or that it rests after a failure.
        QString status;
    };
    /// @brief Every backend with its current status.
    QList<BackendInfo> GetBackends() const;

    /// @brief Name of the backend that served the last search, for the UI.
    QString GetLastSearchBackend() const { return lastSearchBackend; }
    /// @brief Name of the backend that served the last stream, for the UI.
    QString GetLastStreamBackend() const { return lastStreamBackend; }

signals:
    void searchStarted(const QString& query);
    void searchFinished(const QString& query, const QList<OnlineEntry>& entries);
    void searchFailed(const QString& query, const QString& error);
    /// @brief The next page loaded by SearchMore().
    void moreResultsFinished(const QString& query, const QList<OnlineEntry>& entries);
    void moreResultsFailed(const QString& query, const QString& error);

    /// @brief Resting backends or the last used ones changed.
    void backendsChanged();

private:
    OnlineSubsystem();

    void Deinitialize() override;

    /// Preferred first, then the rest in default order; resting ones go last.
    QList<OnlineBackend*> GetOrder(bool bSearch) const;

    void TrySearch(quint64 generation, const QString& query, int count, QList<OnlineBackend*> remaining, QString errors);
    void TryResolve(const QString& pageUrl, QList<OnlineBackend*> remaining, QString errors);
    void FinishResolve(const QString& pageUrl, const QUrl& stream, const QString& error);

    /// Rests a backend after a failure so the next requests do not wait for it.
    void Penalize(OnlineBackend* backend, const OnlineError& error);
    bool IsResting(const OnlineBackend* backend) const;

    static QDateTime ParseStreamExpiry(const QUrl& stream);

    QList<OnlineBackend*> backends;
    QHash<QString, QDateTime> restingUntil;

    quint64 searchGeneration = 0;
    quint64 nextGeneration = 1;
    OnlineBackend* searchBackend = nullptr;

    /// What SearchMore continues.
    OnlineBackend* pagingBackend = nullptr;
    QString pagingQuery;
    quint64 moreGeneration = 0;

    struct PendingCallback
    {
        QPointer<QObject> context;
        StreamCallback callback;
    };

    struct CachedStream
    {
        QUrl url;
        QDateTime expires;
        OnlineBackend* backend = nullptr;
    };

    QHash<QString, QList<PendingCallback>> pendingResolves;
    QHash<QString, CachedStream> streamCache;

    QString lastSearchBackend;
    QString lastStreamBackend;
};
