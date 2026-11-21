//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QByteArray>
#include <QFlags>
#include <QObject>
#include <QUrl>

/// @brief What actually plays the audio: PlayerSubsystem gives it commands and listens to its signals.
///
/// The subsystem owns the queue, the library and every decision; a backend only turns one source into sound.
/// A backend does what it can: GetCapabilities() says which optional parts work, and asking for one it lacks does
/// nothing, so a backend that can only play is still a complete backend.
/// Everything is called and every signal is emitted on the main thread.
class AudioBackend : public QObject
{
    Q_OBJECT

public:
    /// @brief Optional parts of a backend.
    enum Capability
    {
        NoCapabilities = 0,
        SourceData = 1 << 0 ///< Hands out copies of the source bytes it reads: sourceDataReceived().
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    /// @brief What the user asked for; loading and running out of data do not change it.
    enum class State
    {
        Stopped,
        Playing,
        Paused
    };

    using QObject::QObject;

    /// @brief Stable id used in the config.
    virtual QString GetId() const = 0;
    /// @brief The optional parts this backend has.
    virtual Capabilities GetCapabilities() const { return NoCapabilities; }

    /// @brief Replaces the source; playback does not start by itself. An empty URL unloads.
    /// @param source A local file or a direct stream.
    virtual void SetSource(const QUrl& source) = 0;

    virtual void Play() = 0;
    virtual void Pause() = 0;
    /// @brief Stops and rewinds; the source stays loaded.
    virtual void Stop() = 0;
    virtual void Seek(qint64 positionMs) = 0;

    virtual State GetState() const = 0;
    virtual qint64 GetPosition() const = 0;
    /// @brief Duration of the source in ms; 0 while unknown.
    virtual qint64 GetDuration() const = 0;

    /// @brief Sets the volume.
    /// @param linear 0..1, already on a linear scale.
    virtual void SetVolume(float linear) = 0;

    /// @brief Sets the output device.
    /// @param deviceId Endpoint id (AudioDevices); empty for the system default. Called again when devices change.
    virtual void SetOutputDevice(const QByteArray& deviceId) = 0;

    /// @brief Asks for sourceDataReceived(), for sources set from now on. Needs Capability::SourceData.
    virtual void SetSourceDataEnabled(bool bEnabled) { Q_UNUSED(bEnabled); }

signals:
    void stateChanged();
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    /// @brief The source is open: Seek() works from now on.
    void loaded();
    /// @brief The source played to its end.
    void finished();
    /// @brief The source cannot be played further.
    void errorOccurred(const QString& message);

    /// @brief A copy of bytes the backend read from the source. The receiver decides what to do with them.
    ///
    /// Reads follow what playback needs, so a seek leaves gaps and rereads repeat ranges.
    /// @param offset Where the bytes are in the source.
    /// @param bytes The bytes.
    void sourceDataReceived(qint64 offset, const QByteArray& bytes);
    /// @brief The size of the source in bytes, when it is known.
    void sourceSizeKnown(qint64 size);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AudioBackend::Capabilities)
