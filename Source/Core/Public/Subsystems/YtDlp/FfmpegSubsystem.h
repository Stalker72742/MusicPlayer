//
// Created by Stalker7274 on 27.09.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QProcess;
class ReleaseAssetDownload;

/// @brief Keeps ffmpeg available for yt-dlp (merging and converting formats).
///
/// Installs the build yt-dlp recommends (github.com/yt-dlp/FFmpeg-Builds) into <AppLocalData>/tools/ffmpeg,
/// unpacked with the system tar (built into Windows 10 1803+). An ffmpeg on PATH is used while no managed copy exists.
/// Not updated once installed.
class FfmpegSubsystem : public Subsystem<FfmpegSubsystem>
{
    Q_OBJECT
    friend class Subsystem<FfmpegSubsystem>;

    Q_PROPERTY(State state READ GetState NOTIFY stateChanged)

public:
    /// @brief Install state of the tool.
    enum class State
    {
        Unknown,      ///< Not checked yet.
        NotInstalled, ///< No ffmpeg found.
        Downloading,  ///< The release archive is being downloaded.
        Extracting,   ///< The archive is being unpacked.
        Installed,    ///< ffmpeg is available.
        Error         ///< See errorOccurred().
    };
    Q_ENUM(State)

    /// @brief Directory with the ffmpeg and ffprobe executables, for yt-dlp's --ffmpeg-location. Empty if not found.
    QString GetLocation() const;
    /// @brief Whether ffmpeg can be used now.
    bool IsAvailable() const { return !GetLocation().isEmpty(); }

    /// @brief Current install state.
    State GetState() const { return state; }

    /// @brief Installs ffmpeg if it is not available. Asynchronous: follow stateChanged().
    void EnsureInstalled();

signals:
    /// @brief The install state changed.
    void stateChanged(FfmpegSubsystem::State state);
    /// @brief Bytes of the release archive received so far.
    void downloadProgress(qint64 received, qint64 total);
    /// @brief The install failed; the state is State::Error.
    void errorOccurred(const QString& message);

private:
    FfmpegSubsystem();

    void Deinitialize() override;

    bool IsBusy() const { return state == State::Downloading || state == State::Extracting; }
    void SetState(State newState);
    void Fail(const QString& message);

    static QString GetToolsDirectory();
    static QString GetManagedDirectory();
    static QString GetExecutableName();
    static QString GetReleaseAssetName();

    void Extract(const QString& archivePath);
    void InstallExtracted(const QString& extractDir);

    QNetworkAccessManager* network = nullptr;
    QPointer<ReleaseAssetDownload> download;
    QPointer<QProcess> extractProcess;

    State state = State::Unknown;
};
