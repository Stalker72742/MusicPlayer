//
// Created by Stalker7274 on 27.09.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QPointer>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QProcess;
class QTimer;
class ReleaseAssetDownload;

/// @brief Keeps yt-dlp available and up to date.
///
/// Uses the official standalone build from github.com/yt-dlp/yt-dlp releases: a single executable
/// with Python bundled, no other dependencies. It is installed into `<AppLocalData>/tools` and preferred
/// over a yt-dlp found on PATH, which is only used while no managed copy exists.
class YtDlpSubsystem : public Subsystem<YtDlpSubsystem>
{
    Q_OBJECT
    friend class Subsystem<YtDlpSubsystem>;

    Q_PROPERTY(State state READ GetState NOTIFY stateChanged)
    Q_PROPERTY(QString localVersion READ GetLocalVersion NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ GetLatestVersion NOTIFY stateChanged)

public:
    /// @brief Install and update state.
    enum class State
    {
        Unknown,         ///< Not checked yet.
        Checking,        ///< Versions are being queried.
        NotInstalled,    ///< No yt-dlp found.
        UpdateAvailable, ///< A newer release exists.
        Downloading,     ///< A release is being downloaded.
        UpToDate,        ///< The latest release is installed.
        Error            ///< See errorOccurred(); an existing executable is still usable.
    };
    Q_ENUM(State)

    /// @brief Managed copy if installed, otherwise yt-dlp from PATH, otherwise empty.
    QString GetExecutablePath() const;
    /// @brief Whether yt-dlp can be used now.
    bool IsAvailable() const { return !GetExecutablePath().isEmpty(); }

    State GetState() const { return state; }
    /// @brief Output of `yt-dlp --version`, once checked.
    QString GetLocalVersion() const { return localVersion; }
    /// @brief Version of the latest GitHub release, once checked.
    QString GetLatestVersion() const { return latestVersion; }

    /// @brief Queries the local and the latest version. Asynchronous; ignored while busy.
    void CheckForUpdates();
    /// @brief Downloads and installs the latest known release (checks first if none is known yet). Asynchronous; ignored while busy.
    void InstallLatest();

    /// @brief Checks and installs if yt-dlp is missing or outdated.
    void EnsureLatest();

    /// @brief Runs EnsureLatest() now and every hour.
    ///
    /// When AppConfigs::SettingsKeys::AutoUpdateYtDlp is off only a missing yt-dlp is installed.
    void StartAutoUpdate();
    /// @brief Value of AppConfigs::SettingsKeys::AutoUpdateYtDlp.
    bool IsAutoUpdateEnabled() const;

    /// @brief Reports a failed yt-dlp run. Every run must report its outcome.
    ///
    /// A failure may mean yt-dlp is outdated, so it triggers EnsureLatest(): the first one right away, each next
    /// one no sooner than N minutes after the previous, where N is the number of failure-triggered checks so far.
    void ReportRunFailure();
    /// @brief Reports a successful yt-dlp run; resets the cooldown of ReportRunFailure().
    void ReportRunSuccess();

signals:
    /// @brief The state or a version changed.
    void stateChanged(YtDlpSubsystem::State state);
    /// @brief Bytes of the release received so far.
    void downloadProgress(qint64 received, qint64 total);
    /// @brief A check or install failed; the state is State::Error.
    void errorOccurred(const QString& message);

private:
    YtDlpSubsystem();

    void Deinitialize() override;

    bool IsBusy() const { return state == State::Checking || state == State::Downloading; }
    void SetState(State newState);
    void Fail(const QString& message);

    QNetworkAccessManager* GetNetwork();
    QString GetManagedExecutablePath() const;
    static QString GetReleaseAssetName();

    /// Newer if a > b; versions are dates like 2025.09.26 with an optional build suffix.
    static bool IsNewerVersion(const QString& a, const QString& b);

    void QueryLocalVersion(std::function<void(const QString& version)> onDone);
    void QueryLatestVersion(std::function<void(const QString& version)> onDone);
    void FinishCheck();

    void DownloadRelease(const QString& version);
    void RunFailureCheck();

    QNetworkAccessManager* network = nullptr;
    QPointer<QProcess> versionProcess;
    QPointer<ReleaseAssetDownload> download;

    QTimer* hourlyTimer = nullptr;
    QTimer* failureCheckTimer = nullptr;
    int failureChecksSent = 0;
    qint64 lastFailureCheckMs = 0;

    State state = State::Unknown;
    QString localVersion;
    QString latestVersion;

    bool bInstallAfterCheck = false;
};
