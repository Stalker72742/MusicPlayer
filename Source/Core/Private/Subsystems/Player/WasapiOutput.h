//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QByteArray>
#include <QSemaphore>
#include <QString>
#include <QThread>

#include <atomic>

class PcmRing;

/// @brief Sends float samples to a Windows audio device through WASAPI in shared mode, on its own thread.
///
/// The format is the device's own rate and channel count as 32-bit float, so Windows converts nothing.
/// When the ring runs out, the device gets silence. The volume is applied here to the samples:
/// the app's slider in the Windows mixer stays untouched.
class WasapiOutput : public QThread
{
    Q_OBJECT

public:
    explicit WasapiOutput(QObject* parent = nullptr);
    ~WasapiOutput() override;

    /// @brief Opens a device and starts the thread, without playing yet. Blocks until the device answers.
    /// @param deviceId Endpoint id; empty for the system default.
    /// @return Empty on success, otherwise why not.
    QString Open(const QByteArray& deviceId);

    /// @brief Where the samples come from; set before Start(), sized by the format Open() found. Must outlive this object.
    void SetRing(PcmRing* value) { ring = value; }

    /// @brief Starts or resumes taking samples.
    void Start();
    /// @brief Stops taking samples; what the device holds waits for Start().
    void Pause();

    /// @param linear 0..1.
    void SetVolume(float linear) { volume = linear; }

    int GetSampleRate() const { return sampleRate; }
    int GetChannels() const { return channels; }
    /// @brief The endpoint actually opened.
    QByteArray GetEndpoint() const { return endpoint; }

    /// @brief Frames handed to the device that it has not played yet.
    int GetQueuedFrames() const { return queuedFrames; }

    /// @brief Bytes of the frame format.
    int GetBytesPerFrame() const { return channels * static_cast<int>(sizeof(float)); }

signals:
    /// @brief The device was unplugged or changed format; open another one.
    void deviceLost();

protected:
    void run() override;

private:
    std::atomic<PcmRing*> ring{nullptr};
    QByteArray requestedId;
    QByteArray endpoint;
    QString openError;
    QSemaphore ready;
    int sampleRate = 0;
    int channels = 0;

    void* stopEvent = nullptr;
    void* commandEvent = nullptr;
    std::atomic<bool> bWantRunning{false};
    std::atomic<float> volume{1.0f};
    std::atomic<int> queuedFrames{0};
};
