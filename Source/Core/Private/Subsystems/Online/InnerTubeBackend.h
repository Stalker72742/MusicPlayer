//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include <QJsonObject>
#include <QPointer>

#include "OnlineBackend.h"

class QNetworkAccessManager;
class QNetworkReply;

/// @brief Search through InnerTube, the internal API of YouTube's own clients.
///
/// One HTTP request, no process, no cookies, no account. Search only - streams need what yt-dlp does.
/// A page has about 20 videos; the next one comes with the continuation token of the previous.
/// The response format is undocumented; when it changes, search falls back to yt-dlp.
class InnerTubeBackend : public OnlineBackend
{
    Q_OBJECT

public:
    using OnlineBackend::OnlineBackend;

    QString GetId() const override { return QStringLiteral("innertube"); }
    QString GetName() const override { return QStringLiteral("InnerTube"); }
    bool CanSearch() const override { return true; }
    QString GetUnavailableReason() const override { return {}; }

    void Search(const QString& query, int count, SearchCallback callback) override;
    void CancelSearch() override;
    bool CanSearchMore() const override { return !continuation.isEmpty(); }
    void SearchMore(SearchCallback callback) override;
    void Shutdown() override;

private:
    /// body without the client context, which is added here.
    void Send(QJsonObject body, SearchCallback callback);

    /// Videos of a page and the token of the next one; false if the response has neither structure.
    static bool ParsePage(const QByteArray& json, QList<OnlineEntry>& entries, QString& nextContinuation);
    static qint64 ParseDuration(const QString& text);

    QNetworkAccessManager* network = nullptr;
    QPointer<QNetworkReply> searchReply;
    QString continuation;
};
