//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>

/// @brief Discord Rich Presence without the Discord SDK.
///
/// The desktop client listens on a local pipe (`\\.\pipe\discord-ipc-0..9` on Windows,
/// `$XDG_RUNTIME_DIR/discord-ipc-N` elsewhere). Frames are `[opcode u32 LE][length u32 LE][JSON]`; after
/// a handshake with the application id the client answers READY, then SET_ACTIVITY shows
/// "Listening to SoundLink" with the track, its progress and, for online tracks, the cover and a link.
///
/// Follows PlayerSubsystem on its own once Start()ed. Discord may start later or restart: the pipe is
/// looked for again every 20 s. Discord accepts about 5 activity updates per 20 s, so updates are coalesced.
/// Turned off with AppConfigs::SettingsKeys::DiscordPresence.
class DiscordSubsystem : public Subsystem<DiscordSubsystem>
{
    Q_OBJECT
    friend class Subsystem<DiscordSubsystem>;

public:
    /// @brief Starts following the player and connects to Discord if the setting allows.
    void Start();

    /// @brief Applies a change of the setting: connects and shows, or clears and disconnects.
    void ApplySetting();

    /// @brief Whether Discord has answered the handshake.
    bool IsConnected() const { return bReady; }

private:
    DiscordSubsystem();

    void Deinitialize() override;

    enum Opcode : quint32
    {
        Handshake = 0,
        Frame = 1,
        Close = 2,
        Ping = 3,
        Pong = 4
    };

    bool IsEnabled() const;

    void Connect();
    void TryPipe(int index);
    void OnConnected();
    void OnDisconnected();
    void OnReadyRead();

    void Send(Opcode opcode, const QJsonObject& payload);

    /// Builds the activity of the current track and sends it when the rate limit allows.
    void ScheduleUpdate();
    void SendActivity();
    QJsonObject BuildActivity() const;

    QLocalSocket socket;
    QByteArray buffer;
    int pipeIndex = 0;
    bool bReady = false;
    bool bStarted = false;

    QTimer reconnectTimer;
    QTimer updateTimer;
    QDateTime lastSent;

    /// What was last sent, without its timestamps (they move with the clock), and the start it had.
    QByteArray lastActivity;
    qint64 lastStartMs = 0;
};
