//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QList>
#include <QTimer>

#include "Library/Track.h"

class QAudioOutput;
class QMediaDevices;
class QMediaPlayer;

/// @brief Plays library tracks with a queue.
///
/// A local file (scanned, or the downloaded copy of an online track) plays directly; otherwise an online track
/// plays the audio stream OnlineSubsystem resolves when it starts (IsLoading() meanwhile).
/// Volume, shuffle, repeat and the output device are kept in the Settings config.
class PlayerSubsystem : public Subsystem<PlayerSubsystem>
{
    Q_OBJECT
    friend class Subsystem<PlayerSubsystem>;

public:
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
    void OnMediaStatusChanged(int status);
    void OnPlayerError(const QString& message);

    /// Resolves the stream of the current online track and starts it at startMs.
    void LoadStream(const QString& pageUrl, qint64 startMs);
    void SetLoading(bool bValue);

    QMediaPlayer* player = nullptr;
    QAudioOutput* audioOutput = nullptr;
    QMediaDevices* devices = nullptr;

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

    float volume = 0.8f;
    bool bShuffle = false;
    RepeatMode repeatMode = RepeatMode::Off;
};
