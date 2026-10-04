//
// Created by Stalker7274 on 04.10.2026.
//

#include "Discord/DiscordSubsystem.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <QtEndian>

#include "AppConfigs.h"
#include "Library/LibrarySubsystem.h"
#include "Player/PlayerSubsystem.h"

namespace
{
    const QString ClientId = QStringLiteral("1556337315482173460");

    constexpr int PipeCount = 10;
    constexpr int ReconnectIntervalMs = 20'000;

    constexpr int MinUpdateIntervalMs = 4'000;
    constexpr int DebounceMs = 300;

    constexpr qint64 SeekThresholdMs = 2'000;

    constexpr int ActivityTypeListening = 2;
    constexpr int StatusDisplayDetails = 2;

    QString PipeName(int index)
    {
#ifdef Q_OS_WIN
        return QStringLiteral("discord-ipc-%1").arg(index);
#else
        QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (dir.isEmpty())
            dir = qEnvironmentVariable("TMPDIR", QStringLiteral("/tmp"));
        return dir + QStringLiteral("/discord-ipc-%1").arg(index);
#endif
    }

    QString Clamp(QString text)
    {
        text = text.simplified();
        if (text.size() < 2)
            text = text.leftJustified(2, QChar(0x2009));
        return text.left(128);
    }
}

DiscordSubsystem::DiscordSubsystem()
{
    reconnectTimer.setSingleShot(true);
    reconnectTimer.setInterval(ReconnectIntervalMs);
    connect(&reconnectTimer, &QTimer::timeout, this, &DiscordSubsystem::Connect);

    updateTimer.setSingleShot(true);
    connect(&updateTimer, &QTimer::timeout, this, &DiscordSubsystem::SendActivity);

    connect(&socket, &QLocalSocket::connected, this, &DiscordSubsystem::OnConnected);
    connect(&socket, &QLocalSocket::disconnected, this, &DiscordSubsystem::OnDisconnected);
    connect(&socket, &QLocalSocket::readyRead, this, &DiscordSubsystem::OnReadyRead);
    connect(&socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (!bReady && socket.state() == QLocalSocket::UnconnectedState)
            TryPipe(pipeIndex + 1);
    });
}

void DiscordSubsystem::Deinitialize()
{
    reconnectTimer.stop();
    updateTimer.stop();

    if (bReady)
    {
        lastActivity.clear();
        Send(Frame, QJsonObject{
                        {"cmd", "SET_ACTIVITY"},
                        {"args", QJsonObject{{"pid", QCoreApplication::applicationPid()}, {"activity", QJsonValue::Null}}},
                        {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                    });
        socket.waitForBytesWritten(500);
    }
    socket.disconnect(this);
    socket.abort();
}

bool DiscordSubsystem::IsEnabled() const
{
    return AppConfigs::GetBool(AppConfigs::SettingsKeys::DiscordPresence, true);
}

void DiscordSubsystem::Start()
{
    if (bStarted)
        return;
    bStarted = true;

    PlayerSubsystem& player = PlayerSubsystem::Get();
    connect(&player, &PlayerSubsystem::currentTrackChanged, this, &DiscordSubsystem::ScheduleUpdate);
    connect(&player, &PlayerSubsystem::playingChanged, this, &DiscordSubsystem::ScheduleUpdate);
    connect(&player, &PlayerSubsystem::loadingChanged, this, &DiscordSubsystem::ScheduleUpdate);
    connect(&player, &PlayerSubsystem::durationChanged, this, &DiscordSubsystem::ScheduleUpdate);
    connect(&player, &PlayerSubsystem::positionChanged, this, [this](qint64 positionMs) {
        const qint64 start = QDateTime::currentMSecsSinceEpoch() - positionMs;
        if (PlayerSubsystem::Get().IsPlaying() && qAbs(start - lastStartMs) > SeekThresholdMs)
            ScheduleUpdate();
    });
    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, &DiscordSubsystem::ScheduleUpdate);

    Connect();
}

void DiscordSubsystem::ApplySetting()
{
    if (IsEnabled())
    {
        Connect();
        ScheduleUpdate();
        return;
    }

    reconnectTimer.stop();
    updateTimer.stop();
    lastActivity.clear();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.disconnectFromServer();
}

void DiscordSubsystem::Connect()
{
    if (!bStarted || !IsEnabled() || socket.state() != QLocalSocket::UnconnectedState)
        return;

    TryPipe(0);
}

void DiscordSubsystem::TryPipe(int index)
{
    if (index >= PipeCount)
    {
        reconnectTimer.start();
        return;
    }

    pipeIndex = index;
    socket.abort();
    socket.connectToServer(PipeName(index));
}

void DiscordSubsystem::OnConnected()
{
    buffer.clear();
    Send(Handshake, QJsonObject{{"v", 1}, {"client_id", ClientId}});
}

void DiscordSubsystem::OnDisconnected()
{
    const bool bWasReady = bReady;
    bReady = false;
    buffer.clear();
    lastActivity.clear();

    if (bWasReady)
        qDebug() << "[Discord] Disconnected";
    if (IsEnabled())
        reconnectTimer.start();
}

void DiscordSubsystem::OnReadyRead()
{
    buffer += socket.readAll();

    while (buffer.size() >= 8)
    {
        const quint32 opcode = qFromLittleEndian<quint32>(buffer.constData());
        const quint32 length = qFromLittleEndian<quint32>(buffer.constData() + 4);
        if (static_cast<quint32>(buffer.size()) < 8 + length)
            return;

        const QByteArray payload = buffer.mid(8, length);
        buffer.remove(0, 8 + length);
        const QJsonObject message = QJsonDocument::fromJson(payload).object();

        switch (opcode)
        {
            case Frame:
            {
                const QString event = message.value("evt").toString();
                if (event == QStringLiteral("READY"))
                {
                    bReady = true;
                    qDebug() << "[Discord] Connected as"
                             << message.value("data").toObject().value("user").toObject().value("username").toString();
                    SendActivity();
                }
                else if (event == QStringLiteral("ERROR"))
                {
                    qWarning() << "[Discord]" << message.value("data").toObject().value("message").toString();
                }
                else if (message.value("cmd").toString() == QStringLiteral("SET_ACTIVITY"))
                {
                    const QJsonObject data = message.value("data").toObject();
                    qDebug() << "[Discord] Activity:" << (data.isEmpty() ? QStringLiteral("cleared") : data.value("details").toString());
                }
                break;
            }
            case Ping:
                Send(Pong, message);
                break;
            case Close:
                qWarning() << "[Discord] Closed:" << message.value("message").toString();
                socket.disconnectFromServer();
                return;
            default:
                break;
        }
    }
}

void DiscordSubsystem::Send(Opcode opcode, const QJsonObject& payload)
{
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray frame(8, Qt::Uninitialized);
    qToLittleEndian<quint32>(opcode, frame.data());
    qToLittleEndian<quint32>(static_cast<quint32>(json.size()), frame.data() + 4);
    socket.write(frame + json);
    socket.flush();
}

void DiscordSubsystem::ScheduleUpdate()
{
    if (!bReady || updateTimer.isActive())
        return;

    const qint64 sinceLast = lastSent.isValid() ? lastSent.msecsTo(QDateTime::currentDateTime()) : MinUpdateIntervalMs;
    updateTimer.start(static_cast<int>(std::max<qint64>(DebounceMs, MinUpdateIntervalMs - sinceLast)));
}

QJsonObject DiscordSubsystem::BuildActivity() const
{
    PlayerSubsystem& player = PlayerSubsystem::Get();
    const Track* track = LibrarySubsystem::Get().FindTrack(player.GetCurrentTrack());
    if (!track)
        return {};

    const QString artist = track->tags.artist.isEmpty() ? track->tags.album : track->tags.artist;

    QJsonObject activity{
        {"type", ActivityTypeListening},
        {"status_display_type", StatusDisplayDetails},
        {"details", Clamp(track->tags.title)},
        {"state", Clamp(player.IsPlaying() ? artist : artist + QStringLiteral(" · Paused"))},
    };

    const qint64 duration = player.GetDuration();
    if (player.IsPlaying() && !player.IsLoading() && duration > 0)
    {
        const qint64 start = QDateTime::currentMSecsSinceEpoch() - player.GetPosition();
        activity.insert("timestamps", QJsonObject{{"start", start}, {"end", start + duration}});
    }

    if (track->stream.thumbnailUrl.startsWith(QStringLiteral("https://")))
    {
        QJsonObject assets{{"large_image", track->stream.thumbnailUrl}};
        if (!track->tags.album.isEmpty())
            assets.insert("large_text", Clamp(track->tags.album));
        activity.insert("assets", assets);
    }

    if (track->IsOnline())
        activity.insert("buttons", QJsonArray{QJsonObject{{"label", "Open on YouTube"}, {"url", track->stream.pageUrl}}});

    return activity;
}

void DiscordSubsystem::SendActivity()
{
    if (!bReady)
        return;

    const QJsonObject activity = IsEnabled() ? BuildActivity() : QJsonObject{};

    QJsonObject withoutTime = activity;
    withoutTime.remove("timestamps");
    const QByteArray serialized = QJsonDocument(withoutTime).toJson(QJsonDocument::Compact)
                                + (activity.contains("timestamps") ? "+time" : "");
    const qint64 start = activity.value("timestamps").toObject().value("start").toInteger();
    if (serialized == lastActivity && qAbs(start - lastStartMs) <= SeekThresholdMs)
        return;
    lastActivity = serialized;
    lastStartMs = start;

    Send(Frame, QJsonObject{
                    {"cmd", "SET_ACTIVITY"},
                    {"args", QJsonObject{{"pid", QCoreApplication::applicationPid()},
                                 {"activity", activity.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activity)}}},
                    {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                });
    lastSent = QDateTime::currentDateTime();
}
