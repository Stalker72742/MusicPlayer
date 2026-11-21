//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QTimer>
#include <QUrl>

#include <memory>

#include "Player/AudioBackend.h"

class FfmpegWorker;
class PcmRing;
class WasapiOutput;

/// @brief Our own playback: FFmpeg reads and decodes on a worker thread, WasapiOutput sends the samples to the device.
///
/// The source goes through an AVIOContext of ours, which is what lets every byte read be copied out
/// (Capability::SourceData). The sound passes through a ring buffer, the place for future DSP.
class FfmpegBackend : public AudioBackend
{
    Q_OBJECT

public:
    explicit FfmpegBackend(QObject* parent = nullptr);
    ~FfmpegBackend() override;

    QString GetId() const override { return QStringLiteral("ffmpeg"); }
    Capabilities GetCapabilities() const override { return SourceData; }

    void SetSource(const QUrl& source) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void Seek(qint64 positionMs) override;
    State GetState() const override { return state; }
    qint64 GetPosition() const override;
    qint64 GetDuration() const override { return duration; }
    void SetVolume(float linear) override;
    void SetOutputDevice(const QByteArray& deviceId) override;
    void SetSourceDataEnabled(bool bEnabled) override { bSourceData = bEnabled; }

private:
    /// False if no device could be opened; the error is reported.
    bool StartWorker(qint64 startMs);
    void StopWorker();
    /// The same source and state on the device that is right now.
    void Reopen();
    void SetState(State value);
    void OnTick();

    QUrl source;
    QByteArray outputDevice;
    std::unique_ptr<PcmRing> ring;
    std::unique_ptr<WasapiOutput> output;
    FfmpegWorker* worker = nullptr;
    QTimer tick;

    State state = State::Stopped;
    float volume = 1.0f;
    bool bSourceData = false;
    qint64 duration = 0;
    qint64 baseMs = 0;
    qint64 lastPosition = -1;
    bool bDecoderFinished = false;
    int drainTicks = 0;

    /// Bumped per worker: queued signals of an older one are dropped.
    quint64 generation = 0;
};
