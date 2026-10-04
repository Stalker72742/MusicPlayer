//
// Created by Stalker7274 on 27.09.2026.
//

#include "YtDlp/YtDlpSubsystem.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#include <algorithm>

#include "AppConfigs.h"
#include "ReleaseAssetDownload.h"

namespace
{
    const QString LatestReleaseApi = QStringLiteral("https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest");
    const QString ReleaseDownloadUrl = QStringLiteral("https://github.com/yt-dlp/yt-dlp/releases/download/%1/%2");
    const QString ChecksumsAsset = QStringLiteral("SHA2-256SUMS");

    constexpr int VersionQueryTimeoutMs = 15'000;
    constexpr int AutoUpdateIntervalMs = 60 * 60 * 1000;
    constexpr qint64 FailureCooldownStepMs = 60 * 1000;
}

YtDlpSubsystem::YtDlpSubsystem()
    : hourlyTimer(new QTimer(this))
    , failureCheckTimer(new QTimer(this))
{
    hourlyTimer->setInterval(AutoUpdateIntervalMs);
    connect(hourlyTimer, &QTimer::timeout, this, [this] {
        if (IsAutoUpdateEnabled())
            EnsureLatest();
    });

    failureCheckTimer->setSingleShot(true);
    connect(failureCheckTimer, &QTimer::timeout, this, &YtDlpSubsystem::RunFailureCheck);
}

void YtDlpSubsystem::Deinitialize()
{
    hourlyTimer->stop();
    failureCheckTimer->stop();

    delete download;

    if (versionProcess)
    {
        versionProcess->kill();
        versionProcess->waitForFinished(1000);
        delete versionProcess;
    }

    delete network;
    network = nullptr;
}

QString YtDlpSubsystem::GetExecutablePath() const
{
    const QString managed = GetManagedExecutablePath();
    if (QFileInfo::exists(managed))
        return managed;

    return QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
}

QString YtDlpSubsystem::GetManagedExecutablePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tools";
#ifdef Q_OS_WIN
    return dir + "/yt-dlp.exe";
#else
    return dir + "/yt-dlp";
#endif
}

QString YtDlpSubsystem::GetReleaseAssetName()
{
    const QString arch = QSysInfo::currentCpuArchitecture();

#if defined(Q_OS_WIN)
    if (arch == "arm64")
        return QStringLiteral("yt-dlp_arm64.exe");
    if (arch == "i386")
        return QStringLiteral("yt-dlp_x86.exe");
    return QStringLiteral("yt-dlp.exe");
#elif defined(Q_OS_MACOS)
    Q_UNUSED(arch);
    return QStringLiteral("yt-dlp_macos");
#elif defined(Q_OS_LINUX)
    if (arch == "arm64")
        return QStringLiteral("yt-dlp_linux_aarch64");
    return QStringLiteral("yt-dlp_linux");
#else
    Q_UNUSED(arch);
    return {};
#endif
}

bool YtDlpSubsystem::IsNewerVersion(const QString& a, const QString& b)
{
    const QStringList aParts = a.split('.');
    const QStringList bParts = b.split('.');

    for (qsizetype i = 0; i < std::max(aParts.size(), bParts.size()); ++i)
    {
        const qlonglong aValue = i < aParts.size() ? aParts[i].toLongLong() : 0;
        const qlonglong bValue = i < bParts.size() ? bParts[i].toLongLong() : 0;

        if (aValue != bValue)
            return aValue > bValue;
    }
    return false;
}

void YtDlpSubsystem::SetState(State newState)
{
    state = newState;
    emit stateChanged(state);
}

void YtDlpSubsystem::Fail(const QString& message)
{
    qWarning() << "[YtDlp]" << message;
    bInstallAfterCheck = false;
    SetState(State::Error);
    emit errorOccurred(message);
}

QNetworkAccessManager* YtDlpSubsystem::GetNetwork()
{
    if (!network)
        network = new QNetworkAccessManager(this);
    return network;
}

void YtDlpSubsystem::StartAutoUpdate()
{
    hourlyTimer->start();

    if (IsAutoUpdateEnabled() || !IsAvailable())
        EnsureLatest();
}

bool YtDlpSubsystem::IsAutoUpdateEnabled() const
{
    return AppConfigs::GetBool(AppConfigs::SettingsKeys::AutoUpdateYtDlp, true);
}

void YtDlpSubsystem::ReportRunFailure()
{
    if (!IsAutoUpdateEnabled())
        return;

    if (failureCheckTimer->isActive())
        return;

    const qint64 readyAt = lastFailureCheckMs + failureChecksSent * FailureCooldownStepMs;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (failureChecksSent == 0 || now >= readyAt)
        RunFailureCheck();
    else
        failureCheckTimer->start(static_cast<int>(readyAt - now));
}

void YtDlpSubsystem::ReportRunSuccess()
{
    failureCheckTimer->stop();
    failureChecksSent = 0;
}

void YtDlpSubsystem::RunFailureCheck()
{
    lastFailureCheckMs = QDateTime::currentMSecsSinceEpoch();
    ++failureChecksSent;

    qDebug() << "[YtDlp] yt-dlp failed, checking for updates; next failure check in" << failureChecksSent << "min";
    EnsureLatest();
}

void YtDlpSubsystem::EnsureLatest()
{
    if (IsBusy())
        return;

    bInstallAfterCheck = true;
    CheckForUpdates();
}

void YtDlpSubsystem::CheckForUpdates()
{
    if (IsBusy())
        return;

    SetState(State::Checking);

    QueryLocalVersion([this](const QString& version) {
        localVersion = version;

        QueryLatestVersion([this](const QString& latest) {
            latestVersion = latest;
            FinishCheck();
        });
    });
}

void YtDlpSubsystem::FinishCheck()
{
    if (latestVersion.isEmpty())
        return;

    if (localVersion.isEmpty())
        SetState(State::NotInstalled);
    else if (IsNewerVersion(latestVersion, localVersion))
        SetState(State::UpdateAvailable);
    else
        SetState(State::UpToDate);

    qDebug() << "[YtDlp] local:" << (localVersion.isEmpty() ? "none" : localVersion) << "latest:" << latestVersion;

    const bool bInstall = bInstallAfterCheck && state != State::UpToDate;
    bInstallAfterCheck = false;

    if (bInstall)
        DownloadRelease(latestVersion);
}

void YtDlpSubsystem::QueryLocalVersion(std::function<void(const QString& version)> onDone)
{
    const QString executable = GetExecutablePath();
    if (executable.isEmpty())
    {
        onDone({});
        return;
    }

    versionProcess = new QProcess(this);
    QProcess* process = versionProcess;

    connect(process, &QProcess::finished, this, [process, onDone](int exitCode, QProcess::ExitStatus status) {
        const QString version = (status == QProcess::NormalExit && exitCode == 0)
            ? QString::fromUtf8(process->readAllStandardOutput()).trimmed()
            : QString();
        process->deleteLater();
        onDone(version);
    });
    connect(process, &QProcess::errorOccurred, this, [process, onDone](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        onDone({});
    });

    QTimer::singleShot(VersionQueryTimeoutMs, process, [process] { process->kill(); });

    process->start(executable, {QStringLiteral("--version")});
}

void YtDlpSubsystem::QueryLatestVersion(std::function<void(const QString& version)> onDone)
{
    QNetworkReply* reply = SendGitHubRequest(GetNetwork(), QUrl(LatestReleaseApi));

    connect(reply, &QNetworkReply::finished, this, [this, reply, onDone] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            Fail(QStringLiteral("Failed to check the latest version: ") + reply->errorString());
            onDone({});
            return;
        }

        const QString tag = QJsonDocument::fromJson(reply->readAll()).object().value("tag_name").toString();
        if (tag.isEmpty())
        {
            Fail(QStringLiteral("Unexpected response from the GitHub API"));
            onDone({});
            return;
        }

        onDone(tag);
    });
}

void YtDlpSubsystem::InstallLatest()
{
    if (IsBusy())
        return;

    if (latestVersion.isEmpty())
    {
        EnsureLatest();
        return;
    }

    DownloadRelease(latestVersion);
}

void YtDlpSubsystem::DownloadRelease(const QString& version)
{
    const QString asset = GetReleaseAssetName();
    if (asset.isEmpty())
    {
        Fail(QStringLiteral("No standalone yt-dlp build for this platform"));
        return;
    }

    SetState(State::Downloading);

    const QString path = GetManagedExecutablePath();
    download = new ReleaseAssetDownload(GetNetwork(), QUrl(ReleaseDownloadUrl.arg(version, asset)),
        QUrl(ReleaseDownloadUrl.arg(version, ChecksumsAsset)), path, this);

    connect(download, &ReleaseAssetDownload::progress, this, &YtDlpSubsystem::downloadProgress);
    connect(download, &ReleaseAssetDownload::finished, this, [this, path, version](bool bSuccess, const QString& error) {
        if (!bSuccess)
        {
            Fail(error);
            return;
        }

        QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeOwner | QFile::ExeUser);

        localVersion = version;
        qDebug() << "[YtDlp] Installed" << localVersion << "to" << path;
        SetState(State::UpToDate);
    });

    download->Start();
}
