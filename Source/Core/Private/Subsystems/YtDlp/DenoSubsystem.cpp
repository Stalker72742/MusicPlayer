//
// Created by Stalker7274 on 04.10.2026.
//

#include "YtDlp/DenoSubsystem.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#include "ReleaseAssetDownload.h"

namespace
{
    const QString ReleaseDownloadUrl = QStringLiteral("https://github.com/denoland/deno/releases/latest/download/%1");
}

DenoSubsystem::DenoSubsystem() = default;

void DenoSubsystem::Deinitialize()
{
    delete download;

    if (extractProcess)
    {
        extractProcess->kill();
        extractProcess->waitForFinished(1000);
        delete extractProcess;
    }

    delete network;
    network = nullptr;
}

QString DenoSubsystem::GetToolsDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tools";
}

QString DenoSubsystem::GetExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("deno.exe");
#else
    return QStringLiteral("deno");
#endif
}

QString DenoSubsystem::GetReleaseAssetName()
{
    const QString arch = QSysInfo::currentCpuArchitecture();

#if defined(Q_OS_WIN)
    if (arch == "x86_64")
        return QStringLiteral("deno-x86_64-pc-windows-msvc.zip");
    if (arch == "arm64")
        return QStringLiteral("deno-aarch64-pc-windows-msvc.zip");
    return {};
#elif defined(Q_OS_MACOS)
    return arch == "arm64" ? QStringLiteral("deno-aarch64-apple-darwin.zip") : QStringLiteral("deno-x86_64-apple-darwin.zip");
#elif defined(Q_OS_LINUX)
    return arch == "arm64" ? QStringLiteral("deno-aarch64-unknown-linux-gnu.zip")
                           : QStringLiteral("deno-x86_64-unknown-linux-gnu.zip");
#else
    Q_UNUSED(arch);
    return {};
#endif
}

QString DenoSubsystem::GetExecutablePath() const
{
    const QString managed = GetToolsDirectory() + "/" + GetExecutableName();
    if (QFileInfo::exists(managed))
        return managed;

    return QStandardPaths::findExecutable(QStringLiteral("deno"));
}

void DenoSubsystem::SetState(State newState)
{
    state = newState;
    emit stateChanged(state);
}

void DenoSubsystem::Fail(const QString& message)
{
    qWarning() << "[Deno]" << message;
    SetState(State::Error);
    emit errorOccurred(message);
}

void DenoSubsystem::EnsureInstalled()
{
    if (IsBusy())
        return;

    if (IsAvailable())
    {
        qDebug() << "[Deno] Using" << GetExecutablePath();
        SetState(State::Installed);
        return;
    }

    SetState(State::NotInstalled);

    const QString asset = GetReleaseAssetName();
    if (asset.isEmpty())
    {
        Fail(QStringLiteral("No deno build for this platform"));
        return;
    }

    if (!network)
        network = new QNetworkAccessManager(this);

    const QString archivePath = GetToolsDirectory() + "/" + asset;

    SetState(State::Downloading);

    download = new ReleaseAssetDownload(network, QUrl(ReleaseDownloadUrl.arg(asset)),
        QUrl(ReleaseDownloadUrl.arg(asset + ".sha256sum")), archivePath, this);

    connect(download, &ReleaseAssetDownload::progress, this, &DenoSubsystem::downloadProgress);
    connect(download, &ReleaseAssetDownload::finished, this, [this, archivePath](bool bSuccess, const QString& error) {
        if (bSuccess)
            Extract(archivePath);
        else
            Fail(error);
    });

    download->Start();
}

void DenoSubsystem::Extract(const QString& archivePath)
{
    SetState(State::Extracting);

    const QString extractDir = GetToolsDirectory() + "/deno-extract";
    QDir(extractDir).removeRecursively();
    QDir().mkpath(extractDir);

#ifdef Q_OS_WIN
    const QString tar = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath("System32/tar.exe");
#else
    const QString tar = QStringLiteral("tar");
#endif

    extractProcess = new QProcess(this);
    QProcess* process = extractProcess;

    connect(process, &QProcess::finished, this,
        [this, process, archivePath, extractDir](int exitCode, QProcess::ExitStatus status) {
            const QString output = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
            process->deleteLater();
            QFile::remove(archivePath);

            const QString unpacked = extractDir + "/" + GetExecutableName();
            const QString target = GetToolsDirectory() + "/" + GetExecutableName();

            if (status != QProcess::NormalExit || exitCode != 0 || !QFileInfo::exists(unpacked))
            {
                QDir(extractDir).removeRecursively();
                Fail(QStringLiteral("Failed to unpack deno: ") + output);
                return;
            }

            QFile::remove(target);
            const bool bMoved = QFile::rename(unpacked, target);
            QDir(extractDir).removeRecursively();
            if (!bMoved)
            {
                Fail(QStringLiteral("Cannot move deno to ") + target);
                return;
            }

            QFile::setPermissions(target, QFile::permissions(target) | QFile::ExeOwner | QFile::ExeUser);
            qDebug() << "[Deno] Installed to" << target;
            SetState(State::Installed);
        });
    connect(process, &QProcess::errorOccurred, this, [this, process, archivePath, tar](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        QFile::remove(archivePath);
        Fail(QStringLiteral("Cannot run ") + tar);
    });

    process->start(tar, {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"), extractDir});
}
