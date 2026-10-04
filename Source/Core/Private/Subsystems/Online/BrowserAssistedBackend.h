//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include <QJsonObject>

#include "YtDlpBackend.h"

class QNetworkAccessManager;

/// @brief Streams through yt-dlp with a PO token from the user's real browser (BrowserBridge).
///
/// YouTube's bot check already passed in the browser, yt-dlp still does the deciphering. Needs the paired
/// extension, a token it has seen (any video played in the browser recently) and deno.
///
/// Cookies stay in the browser unless needed and allowed (AppConfigs::SettingsKeys::OnlineBrowserCookies,
/// plus a switch in the extension): a logged-in browser's token only works with that session's cookies,
/// and a token-only attempt that hits a sign-in wall is retried with them. Cookies go to a temporary file
/// deleted right after the run.
///
/// yt-dlp prints a URL even for a token YouTube does not accept, so the stream is probed before it is handed out.
class BrowserAssistedBackend : public OnlineBackend
{
    Q_OBJECT

public:
    /// @param ytDlp The yt-dlp backend that does the actual resolving.
    /// @param parent QObject parent.
    explicit BrowserAssistedBackend(YtDlpBackend* ytDlp, QObject* parent = nullptr);

    QString GetId() const override { return QStringLiteral("browser"); }
    QString GetName() const override { return QStringLiteral("Browser extension"); }
    bool CanResolve() const override { return true; }
    QString GetUnavailableReason() const override;

    void ResolveStream(const QString& pageUrl, StreamCallback callback) override;
    void Shutdown() override;

private:
    static bool AreCookiesAllowed();

    void RunWithCookies(const QString& pageUrl, const QString& poToken, StreamCallback callback);
    static QString WriteCookiesFile(const QJsonObject& result);

    /// Resolves with yt-dlp, then checks that the stream answers before calling back.
    void ResolveChecked(const QString& pageUrl, const YtDlpBackend::Credentials& credentials, StreamCallback callback);
    void Probe(const QUrl& stream, std::function<void(const OnlineError& error)> onDone);

    YtDlpBackend* ytDlp;
    QNetworkAccessManager* network = nullptr;
};
