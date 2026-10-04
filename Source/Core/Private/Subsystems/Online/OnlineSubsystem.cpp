//
// Created by Stalker7274 on 03.10.2026.
//

#include "Online/OnlineSubsystem.h"

#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

#include <QTomlUtils/QTomlUtils.h>

#include "AppConfigs.h"
#include "BrowserAssistedBackend.h"
#include "InnerTubeBackend.h"
#include "YtDlpBackend.h"

namespace
{
    constexpr qint64 ExpiryMarginSec = 15 * 60;
    constexpr qint64 DefaultStreamLifetimeSec = 60 * 60;

    constexpr qint64 RestSec = 10 * 60;
}

OnlineSubsystem::OnlineSubsystem()
{
    auto* ytDlp = new YtDlpBackend(this);

    backends = {
        new BrowserAssistedBackend(ytDlp, this),
        new InnerTubeBackend(this),
        ytDlp,
    };

    const QDir tmp(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tmp");
    for (const QString& name : tmp.entryList({QStringLiteral("cookies-*.txt")}, QDir::Files))
        QFile::remove(tmp.filePath(name));
}

void OnlineSubsystem::Deinitialize()
{
    searchGeneration = 0;
    pendingResolves.clear();

    for (OnlineBackend* backend : backends)
        backend->Shutdown();
}

QList<OnlineBackend*> OnlineSubsystem::GetOrder(bool bSearch) const
{
    const QString& key = bSearch ? AppConfigs::SettingsKeys::OnlineSearchSource : AppConfigs::SettingsKeys::OnlineStreamSource;
    const QString preferred = QTomlUtils::FindPropertyValue<QString>(AppConfigs::Settings, key).value_or(QString());

    QList<OnlineBackend*> order;
    for (OnlineBackend* backend : backends)
    {
        if (bSearch ? backend->CanSearch() : backend->CanResolve())
            order.append(backend);
    }

    std::stable_sort(order.begin(), order.end(), [this, &preferred](const OnlineBackend* a, const OnlineBackend* b) {
        const bool aResting = IsResting(a);
        if (aResting != IsResting(b))
            return !aResting;
        return a->GetId() == preferred && b->GetId() != preferred;
    });
    return order;
}

bool OnlineSubsystem::IsResting(const OnlineBackend* backend) const
{
    const auto it = restingUntil.constFind(backend->GetId());
    return it != restingUntil.cend() && QDateTime::currentDateTime() < *it;
}

void OnlineSubsystem::Penalize(OnlineBackend* backend, const OnlineError& error)
{
    if (error.kind != OnlineError::Blocked && error.kind != OnlineError::Failed)
        return;

    qWarning() << "[Online]" << backend->GetName() << "failed, trying it last for a while:" << error.message;
    restingUntil.insert(backend->GetId(), QDateTime::currentDateTime().addSecs(RestSec));
    emit backendsChanged();
}

QList<OnlineSubsystem::BackendInfo> OnlineSubsystem::GetBackends() const
{
    QList<BackendInfo> infos;
    for (const OnlineBackend* backend : backends)
    {
        BackendInfo info{backend->GetId(), backend->GetName(), backend->CanSearch(), backend->CanResolve()};
        info.status = backend->GetUnavailableReason();
        if (info.status.isEmpty() && IsResting(backend))
            info.status = QStringLiteral("Failed recently, tried last for a few minutes");
        infos.append(info);
    }
    return infos;
}

void OnlineSubsystem::Search(const QString& query, int count)
{
    const QString trimmed = query.simplified();
    if (trimmed.isEmpty())
        return;

    CancelSearch();
    const quint64 generation = nextGeneration++;
    searchGeneration = generation;

    emit searchStarted(trimmed);
    TrySearch(generation, trimmed, count, GetOrder(true), {});
}

void OnlineSubsystem::CancelSearch()
{
    if (searchBackend)
        searchBackend->CancelSearch();
    if (pagingBackend && moreGeneration)
        pagingBackend->CancelSearch();

    searchBackend = nullptr;
    searchGeneration = 0;
    pagingBackend = nullptr;
    moreGeneration = 0;
}

bool OnlineSubsystem::CanSearchMore() const
{
    return pagingBackend && !IsSearching() && pagingBackend->CanSearchMore();
}

void OnlineSubsystem::SearchMore()
{
    if (!CanSearchMore() || IsSearchingMore())
        return;

    const quint64 generation = nextGeneration++;
    moreGeneration = generation;

    OnlineBackend* backend = pagingBackend;
    backend->SearchMore([=, this](const QList<OnlineEntry>& entries, const OnlineError& error) {
        if (generation != moreGeneration)
            return;
        moreGeneration = 0;

        if (!error.IsOk())
        {
            qWarning() << "[Online]" << backend->GetName() << "could not load more results:" << error.message;
            emit moreResultsFailed(pagingQuery, error.message);
            return;
        }

        qDebug() << "[Online]" << backend->GetName() << "found" << entries.size() << "more entries for" << pagingQuery;
        emit moreResultsFinished(pagingQuery, entries);
    });
}

void OnlineSubsystem::TrySearch(quint64 generation, const QString& query, int count, QList<OnlineBackend*> remaining, QString errors)
{
    if (generation != searchGeneration)
        return;

    if (remaining.isEmpty())
    {
        searchBackend = nullptr;
        searchGeneration = 0;
        emit searchFailed(query, errors.isEmpty() ? QStringLiteral("No search backend is available") : errors);
        return;
    }

    OnlineBackend* backend = remaining.takeFirst();
    if (const QString reason = backend->GetUnavailableReason(); !reason.isEmpty())
    {
        TrySearch(generation, query, count, remaining, errors);
        return;
    }

    searchBackend = backend;
    backend->Search(query, count, [=, this](const QList<OnlineEntry>& entries, const OnlineError& error) {
        if (generation != searchGeneration)
            return;

        if (error.IsOk())
        {
            searchBackend = nullptr;
            searchGeneration = 0;
            restingUntil.remove(backend->GetId());
            lastSearchBackend = backend->GetName();
            pagingBackend = backend;
            pagingQuery = query;
            emit backendsChanged();

            qDebug() << "[Online]" << backend->GetName() << "found" << entries.size() << "entries for" << query;
            emit searchFinished(query, entries);
            return;
        }

        Penalize(backend, error);
        const QString combined = (errors.isEmpty() ? QString() : errors + "\n") + backend->GetName() + ": " + error.message;
        TrySearch(generation, query, count, remaining, combined);
    });
}

void OnlineSubsystem::ResolveStream(const QString& pageUrl, QObject* context, StreamCallback callback)
{
    const auto cached = streamCache.constFind(pageUrl);
    if (cached != streamCache.cend() && QDateTime::currentDateTime().secsTo(cached->expires) > ExpiryMarginSec)
    {
        const QUrl stream = cached->url;
        QTimer::singleShot(0, context, [callback, stream] { callback(stream, {}); });
        return;
    }

    QList<PendingCallback>& waiting = pendingResolves[pageUrl];
    waiting.append({context, std::move(callback)});
    if (waiting.size() == 1)
        TryResolve(pageUrl, GetOrder(false), {});
}

void OnlineSubsystem::TryResolve(const QString& pageUrl, QList<OnlineBackend*> remaining, QString errors)
{
    if (!pendingResolves.contains(pageUrl))
        return;

    if (remaining.isEmpty())
    {
        FinishResolve(pageUrl, {}, errors.isEmpty() ? QStringLiteral("No stream backend is available") : errors);
        return;
    }

    OnlineBackend* backend = remaining.takeFirst();
    if (const QString reason = backend->GetUnavailableReason(); !reason.isEmpty())
    {
        TryResolve(pageUrl, remaining, errors);
        return;
    }

    backend->ResolveStream(pageUrl, [=, this](const QUrl& stream, const OnlineError& error) {
        if (error.IsOk())
        {
            restingUntil.remove(backend->GetId());
            lastStreamBackend = backend->GetName();
            emit backendsChanged();

            qDebug() << "[Online]" << backend->GetName() << "resolved" << pageUrl;
            streamCache.insert(pageUrl, {stream, ParseStreamExpiry(stream), backend});
            FinishResolve(pageUrl, stream, {});
            return;
        }

        const QString combined = (errors.isEmpty() ? QString() : errors + "\n") + backend->GetName() + ": " + error.message;
        if (error.kind == OnlineError::NotFound)
        {
            FinishResolve(pageUrl, {}, error.message);
            return;
        }

        qDebug() << "[Online]" << backend->GetName() << "could not resolve" << pageUrl << ":" << error.message;
        Penalize(backend, error);
        TryResolve(pageUrl, remaining, combined);
    });
}

void OnlineSubsystem::FinishResolve(const QString& pageUrl, const QUrl& stream, const QString& error)
{
    const QList<PendingCallback> waiting = pendingResolves.take(pageUrl);
    for (const PendingCallback& request : waiting)
    {
        if (request.context)
            request.callback(stream, error);
    }
}

void OnlineSubsystem::ReportStreamFailed(const QString& pageUrl)
{
    const CachedStream cached = streamCache.take(pageUrl);
    if (cached.backend)
        Penalize(cached.backend, OnlineError::Make(OnlineError::Failed, QStringLiteral("its stream could not be played")));
}

void OnlineSubsystem::GuessArtistAndTitle(const OnlineEntry& entry, QString& artist, QString& title)
{
    static const QRegularExpression separator(QStringLiteral("\\s+[-–—]\\s+"));

    const QRegularExpressionMatch match = separator.match(entry.title);
    if (match.hasMatch() && match.capturedStart() > 0)
    {
        artist = entry.title.left(match.capturedStart()).trimmed();
        title = entry.title.mid(match.capturedEnd()).trimmed();
        return;
    }

    artist = entry.channel;
    artist.remove(QRegularExpression(QStringLiteral("\\s+-\\s+Topic$")));
    title = entry.title;
}

QDateTime OnlineSubsystem::ParseStreamExpiry(const QUrl& stream)
{
    bool bOk = false;
    const qint64 expire = QUrlQuery(stream).queryItemValue(QStringLiteral("expire")).toLongLong(&bOk);
    if (bOk && expire > 0)
        return QDateTime::fromSecsSinceEpoch(expire);

    return QDateTime::currentDateTime().addSecs(DefaultStreamLifetimeSec);
}
