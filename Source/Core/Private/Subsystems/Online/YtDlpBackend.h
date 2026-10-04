//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include <QPointer>
#include <QProcess>

#include "OnlineBackend.h"

/// @brief yt-dlp as a backend: YouTube search and audio streams. The fallback for everything.
///
/// Uses deno (DenoSubsystem) when installed; without a JavaScript runtime yt-dlp is limited to clients
/// that need no challenge solving. Every run reports its outcome to YtDlpSubsystem.
class YtDlpBackend : public OnlineBackend
{
    Q_OBJECT

public:
    /// @brief What a browser can lend yt-dlp (BrowserAssistedBackend).
    ///
    /// A PO token goes either with the visitor data it was minted for (logged out) or with the cookies
    /// of the session (logged in).
    struct Credentials
    {
        QString poToken;     ///< GVS PO token from the browser.
        QString visitorData; ///< Visitor data the token was minted for; empty when cookies are used.
        QString cookiesFile; ///< Netscape cookies file; empty when visitor data is used.
    };

    using OnlineBackend::OnlineBackend;

    QString GetId() const override { return QStringLiteral("ytdlp"); }
    QString GetName() const override { return QStringLiteral("yt-dlp"); }
    bool CanSearch() const override { return true; }
    bool CanResolve() const override { return true; }
    QString GetUnavailableReason() const override;

    void Search(const QString& query, int count, SearchCallback callback) override;
    void CancelSearch() override;

    /// @brief Whether more results may exist.
    ///
    /// yt-dlp has no paging: SearchMore() runs the search again for more results and returns only the new ones.
    bool CanSearchMore() const override;
    void SearchMore(SearchCallback callback) override;

    void ResolveStream(const QString& pageUrl, StreamCallback callback) override;
    void Shutdown() override;

    /// @brief ResolveStream() with credentials lent by the browser.
    void ResolveStreamWith(const QString& pageUrl, const Credentials& credentials, StreamCallback callback);

    /// @brief yt-dlp's `-f` for AppConfigs::SettingsKeys::AudioQuality, for streams and downloads.
    static QString GetAudioFormat();

    /// @brief Sorts yt-dlp's error message into an OnlineError kind.
    static OnlineError ClassifyError(const QString& message);

private:
    /// onDone gets stdout or an error; every run reports to YtDlpSubsystem.
    QProcess* Run(const QStringList& args, int timeoutMs,
        std::function<void(const QByteArray& output, const OnlineError& error)> onDone);

    static QList<OnlineEntry> ParseSearchResults(const QByteArray& json);

    void RunSearch(const QString& query, int count, int skip, SearchCallback callback);

    QPointer<QProcess> searchProcess;
    QString lastQuery;
    int fetched = 0;
    bool bExhausted = true;
    QList<QPointer<QProcess>> streamProcesses;
};
