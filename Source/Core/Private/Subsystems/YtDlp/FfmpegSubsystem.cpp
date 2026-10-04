//
// Created by Stalker7274 on 27.09.2026.
//

#include "YtDlp/FfmpegSubsystem.h"

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
    const QString ReleaseDownloadUrl = QStringLiteral("https://github.com/yt-dlp/FFmpeg-Builds/releases/download/latest/%1");
    const QString ChecksumsAsset = QStringLiteral("checksums.sha256");
}

FfmpegSubsystem::FfmpegSubsystem() = default;

void FfmpegSubsystem::Deinitialize()
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

QString FfmpegSubsystem::GetToolsDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tools";
}

QString FfmpegSubsystem::GetManagedDirectory()
{
    return GetToolsDirectory() + "/ffmpeg";
}

QString FfmpegSubsystem::GetExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("ffmpeg.exe");
#else
    return QStringLiteral("ffmpeg");
#endif
}

QString FfmpegSubsystem::GetReleaseAssetName()
{
    const QString arch = QSysInfo::currentCpuArchitecture();

#if defined(Q_OS_WIN)
    if (arch == "arm64")
        return QStringLiteral("ffmpeg-master-latest-winarm64-gpl-shared.zip");
    if (arch == "i386")
        return QStringLiteral("ffmpeg-master-latest-win32-gpl-shared.zip");
    return QStringLiteral("ffmpeg-master-latest-win64-gpl-shared.zip");
#elif defined(Q_OS_LINUX)
    if (arch == "arm64")
        return QStringLiteral("ffmpeg-master-latest-linuxarm64-gpl.tar.xz");
    return QStringLiteral("ffmpeg-master-latest-linux64-gpl.tar.xz");
#else
    Q_UNUSED(arch);
    return {};
#endif
}

QString FfmpegSubsystem::GetLocation() const
{
    const QString managedBin = GetManagedDirectory() + "/bin";
    if (QFileInfo::exists(managedBin + "/" + GetExecutableName()))
        return managedBin;

    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return onPath.isEmpty() ? QString() : QFileInfo(onPath).absolutePath();
}

void FfmpegSubsystem::SetState(State newState)
{
    state = newState;
    emit stateChanged(state);
}

void FfmpegSubsystem::Fail(const QString& message)
{
    qWarning() << "[Ffmpeg]" << message;
    SetState(State::Error);
    emit errorOccurred(message);
}

void FfmpegSubsystem::EnsureInstalled()
{
    if (IsBusy())
        return;

    if (IsAvailable())
    {
        qDebug() << "[Ffmpeg] Using" << GetLocation();
        SetState(State::Installed);
        return;
    }

    SetState(State::NotInstalled);

    const QString asset = GetReleaseAssetName();
    if (asset.isEmpty())
    {
        Fail(QStringLiteral("No ffmpeg build for this platform"));
        return;
    }

    if (!network)
        network = new QNetworkAccessManager(this);

    const QString archivePath = GetToolsDirectory() + "/" + asset;

    SetState(State::Downloading);

    download = new ReleaseAssetDownload(network, QUrl(ReleaseDownloadUrl.arg(asset)),
        QUrl(ReleaseDownloadUrl.arg(ChecksumsAsset)), archivePath, this);

    connect(download, &ReleaseAssetDownload::progress, this, &FfmpegSubsystem::downloadProgress);
    connect(download, &ReleaseAssetDownload::finished, this, [this, archivePath](bool bSuccess, const QString& error) {
        if (bSuccess)
            Extract(archivePath);
        else
            Fail(error);
    });

    download->Start();
}

void FfmpegSubsystem::Extract(const QString& archivePath)
{
    SetState(State::Extracting);

    const QString extractDir = GetToolsDirectory() + "/ffmpeg-extract";
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

            if (status != QProcess::NormalExit || exitCode != 0)
            {
                QDir(extractDir).removeRecursively();
                Fail(QStringLiteral("Failed to unpack ffmpeg: ") + output);
                return;
            }

            InstallExtracted(extractDir);
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

void FfmpegSubsystem::InstallExtracted(const QString& extractDir)
{
    QString unpackedRoot;
    for (const QFileInfo& entry : QDir(extractDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        if (QFileInfo::exists(entry.absoluteFilePath() + "/bin/" + GetExecutableName()))
        {
            unpackedRoot = entry.absoluteFilePath();
            break;
        }
    }

    if (unpackedRoot.isEmpty())
    {
        QDir(extractDir).removeRecursively();
        Fail(QStringLiteral("Unexpected ffmpeg archive layout"));
        return;
    }

    const QString managedDir = GetManagedDirectory();
    QDir(managedDir).removeRecursively();

    if (!QDir().rename(unpackedRoot, managedDir))
    {
        QDir(extractDir).removeRecursively();
        Fail(QStringLiteral("Cannot move ffmpeg to ") + managedDir);
        return;
    }

    QDir(extractDir).removeRecursively();

    qDebug() << "[Ffmpeg] Installed to" << managedDir;
    SetState(State::Installed);
}
