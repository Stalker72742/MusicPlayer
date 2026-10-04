//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QProcess;
class ReleaseAssetDownload;

/// @brief Keeps a JavaScript runtime (deno) for yt-dlp.
///
/// YouTube extraction without one is deprecated and limited to
/// clients that need no challenge solving, which also rules out using a browser's PO token.
/// Installs deno (github.com/denoland/deno releases) next to the managed yt-dlp, where yt-dlp finds it
/// without any arguments. A deno on PATH is used while no managed copy exists. Not updated once installed.
class DenoSubsystem : public Subsystem<DenoSubsystem>
{
    Q_OBJECT
    friend class Subsystem<DenoSubsystem>;

    Q_PROPERTY(State state READ GetState NOTIFY stateChanged)

public:
    /// @brief Install state of the tool.
    enum class State
    {
        Unknown,      ///< Not checked yet.
        NotInstalled, ///< No deno found.
        Downloading,  ///< The release archive is being downloaded.
        Extracting,   ///< The archive is being unpacked.
        Installed,    ///< deno is available.
        Error         ///< See errorOccurred().
    };
    Q_ENUM(State)

    /// @brief Path of the deno executable: the managed copy, else one on PATH; empty if not found.
    QString GetExecutablePath() const;
    /// @brief Whether deno can be used now.
    bool IsAvailable() const { return !GetExecutablePath().isEmpty(); }

    /// @brief Current install state.
    State GetState() const { return state; }

    /// @brief Installs deno if it is not available. Asynchronous: follow stateChanged().
    void EnsureInstalled();

signals:
    /// @brief The install state changed.
    void stateChanged(DenoSubsystem::State state);
    /// @brief Bytes of the release archive received so far.
    void downloadProgress(qint64 received, qint64 total);
    /// @brief The install failed; the state is State::Error.
    void errorOccurred(const QString& message);

private:
    DenoSubsystem();

    void Deinitialize() override;

    bool IsBusy() const { return state == State::Downloading || state == State::Extracting; }
    void SetState(State newState);
    void Fail(const QString& message);

    static QString GetToolsDirectory();
    static QString GetExecutableName();
    static QString GetReleaseAssetName();

    void Extract(const QString& archivePath);

    QNetworkAccessManager* network = nullptr;
    QPointer<ReleaseAssetDownload> download;
    QPointer<QProcess> extractProcess;

    State state = State::Unknown;
};
