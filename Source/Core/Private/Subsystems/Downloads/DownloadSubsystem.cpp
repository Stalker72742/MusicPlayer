//
// Created by Stalker7274 on 04.10.2026.
//

#include "Downloads/DownloadSubsystem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <QTomlUtils/QTomlUtils.h>

#include "../Online/YtDlpBackend.h"
#include "AppConfigs.h"
#include "Library/LibrarySubsystem.h"
#include "YtDlp/DenoSubsystem.h"
#include "YtDlp/FfmpegSubsystem.h"
#include "YtDlp/YtDlpSubsystem.h"

namespace
{
    constexpr int MaxParallel = 2;
    constexpr int StoreVersion = 1;
    constexpr int SaveDelayMs = 1000;

    constexpr int StallTimeoutMs = 120'000;


    const QString ProgressMarker = QStringLiteral("SLPROGRESS");

    QString SanitizeFileName(QString name)
    {
        static const QRegularExpression forbidden(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]"));
        name.replace(forbidden, QStringLiteral("_"));
        name = name.simplified();
        while (name.endsWith('.') || name.endsWith(' '))
            name.chop(1);
        return name.left(150);
    }
}

DownloadSubsystem::DownloadSubsystem()
{
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(SaveDelayMs);
    connect(&saveTimer, &QTimer::timeout, this, &DownloadSubsystem::Save);
}

void DownloadSubsystem::Initialize()
{
    if (bInitialized)
        return;

    bInitialized = true;
    Load();
    StartNext();
}

void DownloadSubsystem::Deinitialize()
{
    for (const QPointer<QProcess>& process : processes)
    {
        if (!process)
            continue;
        process->disconnect();
        process->kill();
        process->waitForFinished(1000);
        delete process;
    }
    processes.clear();

    for (Job& job : jobs)
    {
        if (job.state == Job::State::Downloading)
        {
            RemoveTempFiles(job.track);
            job.state = Job::State::Queued;
        }
    }

    saveTimer.stop();
    Save();
}

QString DownloadSubsystem::GetFolder() const
{
    return QTomlUtils::FindPropertyValue<QString>(AppConfigs::Settings, AppConfigs::SettingsKeys::DownloadFolder)
        .value_or(QString());
}

QString DownloadSubsystem::GetTempDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads-tmp";
}

const DownloadSubsystem::Job* DownloadSubsystem::FindJob(TrackId track) const
{
    for (const Job& job : jobs)
    {
        if (job.track == track)
            return &job;
    }
    return nullptr;
}

DownloadSubsystem::Job* DownloadSubsystem::FindMutable(TrackId track)
{
    return const_cast<Job*>(FindJob(track));
}

int DownloadSubsystem::GetActiveCount() const
{
    return static_cast<int>(std::count_if(jobs.cbegin(), jobs.cend(), [](const Job& job) {
        return job.state == Job::State::Queued || job.state == Job::State::Downloading;
    }));
}

int DownloadSubsystem::Enqueue(const QList<TrackId>& tracks)
{
    LibrarySubsystem& library = LibrarySubsystem::Get();

    int queued = 0;
    for (TrackId track : tracks)
    {
        const Track* found = library.FindTrack(track);
        if (!found || !CanDownload(*found))
            continue;

        Job* existing = FindMutable(track);
        if (existing && (existing->state == Job::State::Queued || existing->state == Job::State::Downloading))
            continue;

        Job job{track};
        if (!found->IsInLibrary())
        {
            library.SetSaved(track, true);
            job.bSavedForDownload = true;
        }

        if (existing)
            *existing = job;
        else
            jobs.append(job);
        ++queued;
    }

    if (queued > 0)
    {
        emit jobsChanged();
        ScheduleSave();
        StartNext();
    }
    return queued;
}

void DownloadSubsystem::Cancel(TrackId track)
{
    const auto it = std::find_if(jobs.begin(), jobs.end(), [track](const Job& job) { return job.track == track; });
    if (it == jobs.end())
        return;

    if (QPointer<QProcess> process = processes.take(track.value))
    {
        process->disconnect();
        process->kill();
        process->waitForFinished(1000);
        process->deleteLater();
    }
    RemoveTempFiles(track);

    const bool bUnsave = it->bSavedForDownload && it->state != Job::State::Done;
    jobs.erase(it);
    if (bUnsave)
        LibrarySubsystem::Get().SetSaved(track, false);
    emit jobsChanged();
    ScheduleSave();
    StartNext();
}

void DownloadSubsystem::Retry(TrackId track)
{
    Job* job = FindMutable(track);
    if (!job || job->state != Job::State::Failed)
        return;

    *job = Job{track};
    emit jobsChanged();
    ScheduleSave();
    StartNext();
}

void DownloadSubsystem::ClearFinished()
{
    const qsizetype before = jobs.size();
    jobs.removeIf([](const Job& job) { return job.state == Job::State::Done || job.state == Job::State::Failed; });
    if (jobs.size() != before)
    {
        emit jobsChanged();
        ScheduleSave();
    }
}

void DownloadSubsystem::RemoveDownload(TrackId track)
{
    const Track* found = LibrarySubsystem::Get().FindTrack(track);
    if (!found || !found->IsOnline() || !found->HasLocalFile())
        return;

    const QString path = found->file.path;
    LibrarySubsystem::Get().SetLocalFile(track, {});
    if (QFile::exists(path) && !QFile::remove(path))
        qWarning() << "[Downloads] Cannot delete" << path << "(in use?)";

    const qsizetype before = jobs.size();
    jobs.removeIf([track](const Job& job) { return job.track == track && job.state == Job::State::Done; });
    if (jobs.size() != before)
        emit jobsChanged();
}

void DownloadSubsystem::StartNext()
{
    if (!bInitialized)
        return;

    for (Job& job : jobs)
    {
        if (processes.size() >= MaxParallel)
            return;
        if (job.state == Job::State::Queued)
            Start(job);
    }
}

void DownloadSubsystem::Start(Job& job)
{
    const TrackId track = job.track;
    const Track* found = LibrarySubsystem::Get().FindTrack(track);
    if (!found || !CanDownload(*found))
    {
        job.state = found ? Job::State::Done : Job::State::Failed;
        job.error = found ? QString() : QStringLiteral("The track is no longer in the library");
        job.finished = QDateTime::currentDateTime();
        emit jobsChanged();
        return;
    }

    const QString executable = YtDlpSubsystem::Get().GetExecutablePath();
    if (executable.isEmpty())
    {
        YtDlpSubsystem::Get().EnsureLatest();
        job.state = Job::State::Failed;
        job.error = QStringLiteral("yt-dlp is not installed yet");
        job.finished = QDateTime::currentDateTime();
        emit jobsChanged();
        return;
    }

    const QString tempDir = GetTempDirectory();
    QDir().mkpath(tempDir);
    RemoveTempFiles(track);

    QStringList args{
        QStringLiteral("-f"), YtDlpBackend::GetAudioFormat(),
        QStringLiteral("--no-playlist"),
        QStringLiteral("--no-warnings"),
        QStringLiteral("--no-mtime"),
        QStringLiteral("--newline"),
        QStringLiteral("--progress"),
        QStringLiteral("--progress-template"),
        QStringLiteral("download:%1 %(progress.downloaded_bytes)s %(progress.total_bytes)s %(progress.total_bytes_estimate)s")
            .arg(ProgressMarker),
        QStringLiteral("--print"), QStringLiteral("after_move:filepath"),
        QStringLiteral("-o"), tempDir + QStringLiteral("/%1.%(ext)s").arg(track.value),
    };

    const QString ffmpeg = FfmpegSubsystem::Get().GetLocation();
    if (!ffmpeg.isEmpty())
        args << QStringLiteral("--ffmpeg-location") << ffmpeg << QStringLiteral("--embed-metadata");

    const QString deno = DenoSubsystem::Get().GetExecutablePath();
    if (!deno.isEmpty())
        args << QStringLiteral("--js-runtimes") << QStringLiteral("deno:") + deno;

    args << found->stream.pageUrl;

    auto* process = new QProcess(this);
    auto* stall = new QTimer(process);
    stall->setSingleShot(true);
    stall->setInterval(StallTimeoutMs);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, track, process, stall] {
        stall->start();
        OnOutput(track, process);
    });
    connect(stall, &QTimer::timeout, this, [this, track, process] {
        process->disconnect();
        process->kill();
        process->deleteLater();
        processes.remove(track.value);
        YtDlpSubsystem::Get().ReportRunFailure();
        Finish(track, {}, QStringLiteral("The download stopped making progress"));
    });
    connect(process, &QProcess::finished, this, [this, track, process](int exitCode, QProcess::ExitStatus status) {
        OnOutput(track, process);
        const QString path = process->property("finalPath").toString();
        const QString stderrText = QString::fromLocal8Bit(process->readAllStandardError());
        process->deleteLater();
        processes.remove(track.value);

        if (status != QProcess::NormalExit || exitCode != 0 || path.isEmpty())
        {
            QString message;
            for (const QString& line : stderrText.split('\n', Qt::SkipEmptyParts))
            {
                if (line.startsWith(QStringLiteral("ERROR:")))
                    message = line.mid(6).trimmed();
            }
            if (message.isEmpty())
                message = QStringLiteral("yt-dlp exited with code %1").arg(exitCode);

            const OnlineError error = YtDlpBackend::ClassifyError(message);
            if (error.kind == OnlineError::Failed || error.kind == OnlineError::Blocked)
                YtDlpSubsystem::Get().ReportRunFailure();
            Finish(track, {}, message);
            return;
        }

        YtDlpSubsystem::Get().ReportRunSuccess();
        Finish(track, path, {});
    });
    connect(process, &QProcess::errorOccurred, this, [this, track, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        processes.remove(track.value);
        Finish(track, {}, QStringLiteral("Cannot start yt-dlp"));
    });

    job.state = Job::State::Downloading;
    job.received = 0;
    job.total = 0;
    job.error.clear();
    processes.insert(track.value, process);
    emit jobsChanged();

    qDebug() << "[Downloads] Downloading" << found->stream.pageUrl;
    stall->start();
    process->start(executable, args);
}

void DownloadSubsystem::OnOutput(TrackId track, QProcess* process)
{
    Job* job = FindMutable(track);

    while (process->canReadLine())
    {
        const QString line = QString::fromUtf8(process->readLine()).trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith(ProgressMarker))
        {
            if (!job)
                continue;

            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            job->received = parts.value(1).toLongLong();
            const qint64 total = parts.value(2).toLongLong();
            job->total = total > 0 ? total : static_cast<qint64>(parts.value(3).toDouble());
            emit progressChanged(track);
            continue;
        }

        process->setProperty("finalPath", line);
    }
}

void DownloadSubsystem::Finish(TrackId track, const QString& downloadedPath, const QString& error)
{
    Job* job = FindMutable(track);

    QString failure = error;
    QString target;

    const Track* found = LibrarySubsystem::Get().FindTrack(track);
    if (failure.isEmpty() && !found)
        failure = QStringLiteral("The track is no longer in the library");

    if (failure.isEmpty())
    {
        const QString folder = GetFolder();
        if (folder.isEmpty() || !QDir().mkpath(folder))
        {
            failure = QStringLiteral("Cannot create the download folder ") + folder;
        }
        else
        {
            target = MakeTargetPath(*found, QFileInfo(downloadedPath).suffix());

            if (!QFile::rename(downloadedPath, target))
            {
                if (QFile::copy(downloadedPath, target))
                    QFile::remove(downloadedPath);
                else
                    failure = QStringLiteral("Cannot move the file to ") + folder;
            }
        }
    }

    RemoveTempFiles(track);

    if (job)
    {
        job->state = failure.isEmpty() ? Job::State::Done : Job::State::Failed;
        job->error = failure;
        job->finished = QDateTime::currentDateTime();
        if (job->state == Job::State::Done && job->total <= 0)
            job->total = job->received = QFileInfo(target).size();
    }

    if (failure.isEmpty())
    {
        qDebug() << "[Downloads] Saved" << target;
        LibrarySubsystem::Get().SetLocalFile(track, target);
    }
    else
    {
        qWarning() << "[Downloads] Failed:" << failure;
    }

    emit jobsChanged();
    ScheduleSave();
    StartNext();
}

QString DownloadSubsystem::MakeTargetPath(const Track& track, const QString& extension) const
{
    const QString base = SanitizeFileName(track.tags.artist.isEmpty() ? track.tags.title
                                                                       : track.tags.artist + " - " + track.tags.title);
    const QString name = base.isEmpty() ? QStringLiteral("Track %1").arg(track.id.value) : base;
    const QString suffix = extension.isEmpty() ? QString() : "." + extension;

    const QDir folder(GetFolder());
    QString path = folder.filePath(name + suffix);
    for (int copy = 2; QFile::exists(path); ++copy)
        path = folder.filePath(QStringLiteral("%1 (%2)%3").arg(name).arg(copy).arg(suffix));
    return path;
}

void DownloadSubsystem::RemoveTempFiles(TrackId track) const
{
    const QDir dir(GetTempDirectory());
    const QString prefix = QString::number(track.value) + ".";
    for (const QString& name : dir.entryList(QDir::Files))
    {
        if (name.startsWith(prefix))
            QFile::remove(dir.filePath(name));
    }
}

void DownloadSubsystem::ScheduleSave()
{
    saveTimer.start();
}

void DownloadSubsystem::Load()
{
    QFile file(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads.json");
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value("version").toInt() != StoreVersion)
        return;

    for (const QJsonValue& value : root.value("jobs").toArray())
    {
        const QJsonObject object = value.toObject();
        const TrackId track{static_cast<quint32>(object.value("track").toInteger())};
        if (!track.IsValid() || FindJob(track))
            continue;

        Job job{track};
        if (object.value("failed").toBool())
        {
            job.state = Job::State::Failed;
            job.error = object.value("error").toString();
        }
        jobs.append(job);
    }

    QDir(GetTempDirectory()).removeRecursively();
}

void DownloadSubsystem::Save()
{
    QJsonArray array;
    for (const Job& job : jobs)
    {
        if (job.state == Job::State::Done)
            continue;

        QJsonObject object{{"track", static_cast<qint64>(job.track.value)}};
        if (job.state == Job::State::Failed)
        {
            object.insert("failed", true);
            object.insert("error", job.error);
        }
        array.append(object);
    }

    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads.json";
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{"version", StoreVersion}, {"jobs", array}}).toJson(QJsonDocument::Compact));
    file.commit();
}
