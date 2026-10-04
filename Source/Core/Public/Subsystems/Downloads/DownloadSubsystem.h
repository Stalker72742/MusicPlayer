//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>

#include "Library/Track.h"

/// @brief Downloads online tracks through a queue, a few at a time, with yt-dlp.
///
/// A finished file goes to the download folder (AppConfigs::SettingsKeys::DownloadFolder) and becomes the track's
/// local copy (LibrarySubsystem::SetLocalFile): the track stays the same, the player just stops streaming it.
///
/// yt-dlp writes into `<AppLocalData>/downloads-tmp` first, so cancelled and failed runs leave nothing behind
/// in the music folders. Queued jobs survive a restart (`<AppLocalData>/downloads.json`).
class DownloadSubsystem : public Subsystem<DownloadSubsystem>
{
    Q_OBJECT
    friend class Subsystem<DownloadSubsystem>;

public:
    /// @brief A download of one track.
    struct Job
    {
        /// @brief Where the job is in the queue.
        enum class State
        {
            Queued,
            Downloading,
            Done,
            Failed
        };

        TrackId track;
        State state = State::Queued;
        qint64 received = 0; ///< Bytes downloaded so far.
        qint64 total = 0;    ///< Total bytes; 0 while unknown.
        QString error;       ///< Why the job failed.
        QDateTime finished;  ///< When the job finished or failed.

        /// @brief A search result saved only so it outlives the search while it downloads; cancelling unsaves it.
        bool bSavedForDownload = false;
    };

    /// @brief Starts the queue saved last time. Call once after the library is loaded.
    void Initialize();

    /// @brief Queues tracks for download.
    ///
    /// Only online tracks without a local copy are taken; tracks already queued are skipped.
    /// @return How many were queued.
    int Enqueue(const QList<TrackId>& tracks);

    /// @brief Drops a queued job or stops a running one (a search result goes back to being one).
    void Cancel(TrackId track);
    /// @brief Queues a failed job again.
    void Retry(TrackId track);

    /// @brief Forgets finished and failed jobs.
    void ClearFinished();

    /// @brief Deletes the downloaded file; the track goes back to streaming.
    void RemoveDownload(TrackId track);

    /// @brief Every job in queue order.
    const QList<Job>& GetJobs() const { return jobs; }
    /// @brief The job of a track, or nullptr.
    const Job* FindJob(TrackId track) const;
    /// @brief Jobs queued or downloading.
    int GetActiveCount() const;

    /// @brief The download folder from the settings.
    QString GetFolder() const;

    /// @brief Whether the track can be downloaded: online and without a local copy.
    static bool CanDownload(const Track& track) { return track.IsOnline() && !track.HasLocalFile(); }

signals:
    /// @brief Jobs added, removed or changed state.
    void jobsChanged();
    /// @brief Received bytes of a running job changed.
    void progressChanged(TrackId track);

private:
    DownloadSubsystem();

    void Deinitialize() override;

    Job* FindMutable(TrackId track);
    void StartNext();
    void Start(Job& job);
    void Finish(TrackId track, const QString& downloadedPath, const QString& error);
    void OnOutput(TrackId track, QProcess* process);

    QString GetTempDirectory() const;
    void RemoveTempFiles(TrackId track) const;

    /// "Artist - Title.m4a" in the download folder, not overwriting anything.
    QString MakeTargetPath(const Track& track, const QString& extension) const;

    void ScheduleSave();
    void Load();
    void Save();

    QList<Job> jobs;
    QHash<quint32, QPointer<QProcess>> processes;
    QTimer saveTimer;
    bool bInitialized = false;
};
