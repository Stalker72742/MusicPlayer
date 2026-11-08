//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QList>
#include <QTimer>

#include <memory>

#include "Library/Track.h"
#include "Player/AudioBackend.h"

class AudioDevices;
class StreamCapture;

/// @brief Plays library tracks with a queue.
///
/// A local file (scanned, or the downloaded copy of an online track) plays directly; otherwise an online track
/// plays the audio stream OnlineSubsystem resolves when it starts (IsLoading() meanwhile).
/// Volume, shuffle, repeat and the output device are kept in the Settings config.
///
/// The sound itself is an AudioBackend's job. Bytes a backend hands out go
/// to a StreamCapture when AppConfigs::SettingsKeys::SaveWhileListening is on: a stream that was read whole
/// becomes the downloaded copy of the track.
class PlayerSubsystem : public Subsystem<PlayerSubsystem>
{
    Q_OBJECT
    friend class Subsystem<PlayerSubsystem>;

public:
    ~PlayerSubsystem();

    /// @brief What happens at the end of a track and of the queue.
    enum class RepeatMode
    {
        Off, ///< The queue plays once.
        All, ///< The queue starts over.
        One  ///< The current track repeats.
    };

    /// @brief Replaces the queue and starts playing `newQueue[index]`.
    void PlayQueue(const QList<TrackId>& newQueue, int index);

    /// @brief Resumes the current track.
    void Play();
    /// @brief Pauses the current track.
    void Pause();
    /// @brief Play() or Pause(), whichever applies.
    void TogglePlay();

    /// @brief Goes to the next track.
    ///
    /// Respects shuffle and repeat (RepeatMode::One only applies when a track ends by itself);
    /// at the end of the queue without repeat stops on the last track, rewound.
    void Next();

    /// @brief Restarts the track if it has played for a few seconds, otherwise goes to the previous one.
    void Previous();

    /// @brief Moves the playback position of the current track.
    void Seek(qint64 positionMs);

    /// @brief Closes the file of the current track if it is this track, so the file can be renamed or deleted.
    ///
    /// Windows does not let a file that is open be renamed or deleted. Follow with RestoreFile().
    /// @return Whether the file was open.
    bool ReleaseFile(TrackId id);
    /// @brief Opens the current track again after ReleaseFile(), where it was and playing if it was.
    ///
    /// The file may have a new path (renamed) or be gone: then an online track streams, a local one is skipped
    /// if it was playing.
    void RestoreFile();

    /// @brief Sets the volume and saves it to the config once it settles.
    /// @param volume 0..1 on a perceptual (logarithmic) scale.
    void SetVolume(float volume);

    /// @brief Applies AppConfigs::SettingsKeys::OutputDevice: that device if present, else the system default.
    void ApplyOutputDevice();
    /// @brief Volume 0..1 on a perceptual scale.
    float GetVolume() const { return volume; }

    /// @brief Enables or disables shuffle; saved to the config.
    void SetShuffle(bool bValue);
    /// @brief Sets the repeat mode; saved to the config.
    void SetRepeatMode(RepeatMode mode);
    bool IsShuffle() const { return bShuffle; }
    RepeatMode GetRepeatMode() const { return repeatMode; }

    /// @brief The tracks of the queue.
    const QList<TrackId>& GetQueue() const { return queue; }

    /// @brief The current track of the queue, or an invalid id.
    TrackId GetCurrentTrack() const;

    /// @brief Whether the track is playing; while loading, whether it will play once loaded.
    bool IsPlaying() const;
    /// @brief Whether the stream of an online track is being resolved.
    bool IsLoading() const { return bLoading; }
    /// @brief Playback position in ms.
    qint64 GetPosition() const;
    /// @brief Duration of the current track in ms.
    qint64 GetDuration() const;

signals:
    void currentTrackChanged();
    void playingChanged();
    void loadingChanged();
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void volumeChanged();
    void shuffleChanged();
    void repeatChanged();
    /// @brief Playback failed; message is for the user.
    void errorOccurred(const QString& message);

private:
    PlayerSubsystem();

    void Deinitialize() override;

    void PlayIndex(int index);

    /// The end of the queue without repeat.
    void StopAtEnd();
    void OnBackendFinished();
    void OnBackendLoaded();
    void OnPlayerError(const QString& message);

    void ConnectBackend();
    void ApplyVolume();

    /// Starts collecting the source being set, if the setting is on and the backend can hand out bytes.
    void StartCapture(const Track& track, const QUrl& stream);
    void OnSourceData(qint64 offset, const QByteArray& bytes);

    /// Resolves the stream of the current online track and starts it at startMs.
    void LoadStream(const QString& pageUrl, qint64 startMs);
    void SetLoading(bool bValue);

    AudioBackend* player = nullptr;
    AudioDevices* devices = nullptr;
    std::unique_ptr<StreamCapture> capture;
    TrackId captureTrack;
    QString captureExtension;

    /// Dragging the volume slider writes the config once it settles.
    QTimer volumeSaveTimer;

    QList<TrackId> queue;
    int currentIndex = -1;

    /// Bumped on every track change: a stream resolved for an older one is dropped.
    quint64 loadRequest = 0;
    bool bLoading = false;
    bool bPlayWhenLoaded = true;
    bool bStreamRetried = false;
    qint64 pendingSeekMs = 0;

    /// Set by ReleaseFile().
    TrackId releasedTrack;
    qint64 releasedPositionMs = 0;
    bool bReleasedPlaying = false;

    float volume = 0.8f;
    bool bShuffle = false;
    RepeatMode repeatMode = RepeatMode::Off;
};
