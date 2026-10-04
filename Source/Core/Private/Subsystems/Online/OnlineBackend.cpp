//
// Created by Stalker7274 on 04.10.2026.
//

#include "OnlineBackend.h"

#include <QTimer>

void OnlineBackend::Search(const QString&, int, SearchCallback callback)
{
    QTimer::singleShot(0, this, [callback] {
        callback({}, OnlineError::Make(OnlineError::Unavailable, QStringLiteral("Search is not supported")));
    });
}

void OnlineBackend::SearchMore(SearchCallback callback)
{
    QTimer::singleShot(0, this, [callback] {
        callback({}, OnlineError::Make(OnlineError::Unavailable, QStringLiteral("No more results")));
    });
}

void OnlineBackend::ResolveStream(const QString&, StreamCallback callback)
{
    QTimer::singleShot(0, this, [callback] {
        callback({}, OnlineError::Make(OnlineError::Unavailable, QStringLiteral("Streams are not supported")));
    });
}
