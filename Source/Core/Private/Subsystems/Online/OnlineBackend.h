//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include <QObject>
#include <QUrl>

#include <functional>

#include "Online/OnlineTypes.h"

/// @brief A way to search or to get the audio stream of a page.
///
/// OnlineSubsystem tries backends in the user's order and falls back to the next one depending on
/// OnlineError::Kind.
class OnlineBackend : public QObject
{
    Q_OBJECT

public:
    /// @brief Result of a search: the entries, or an error.
    using SearchCallback = std::function<void(const QList<OnlineEntry>& entries, const OnlineError& error)>;
    /// @brief Result of a stream resolve: the direct audio URL, or an error.
    using StreamCallback = std::function<void(const QUrl& stream, const OnlineError& error)>;

    using QObject::QObject;

    /// @brief Stable id used in the config: "innertube", "ytdlp", "browser".
    virtual QString GetId() const = 0;
    /// @brief Name for the UI.
    virtual QString GetName() const = 0;

    /// @brief Whether Search() is supported.
    virtual bool CanSearch() const { return false; }
    /// @brief Whether ResolveStream() is supported.
    virtual bool CanResolve() const { return false; }

    /// @brief Empty while the backend can be used right now; otherwise why not.
    virtual QString GetUnavailableReason() const = 0;

    /// @brief Searches; calls back exactly once, unless cancelled or the backend shut down.
    /// @param query Search text.
    /// @param count Number of results wanted.
    /// @param callback Receives the results.
    virtual void Search(const QString& query, int count, SearchCallback callback);
    /// @brief Cancels the running search or SearchMore(); its callback is not called.
    virtual void CancelSearch() {}

    /// @brief Whether the last successful Search() has a next page.
    virtual bool CanSearchMore() const { return false; }
    /// @brief Loads the next page of the last successful Search(). Cancelled by CancelSearch() too.
    virtual void SearchMore(SearchCallback callback);
    /// @brief Finds the direct audio stream of a page; calls back exactly once, unless the backend shut down.
    virtual void ResolveStream(const QString& pageUrl, StreamCallback callback);

    /// @brief Stops processes and network; called from OnlineSubsystem::Deinitialize().
    virtual void Shutdown() {}
};
