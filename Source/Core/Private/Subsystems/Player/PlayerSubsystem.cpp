//
// Created by Stalker7274 on 03.10.2026.
//

#include "Player/PlayerSubsystem.h"

#include <QAudio>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QRandomGenerator>
#include <QUrl>

#include <QTomlUtils/QTomlUtils.h>

#include <cmath>

#include "AppConfigs.h"
#include "Library/LibrarySubsystem.h"
#include "Online/OnlineSubsystem.h"

namespace
{
    constexpr qint64 RestartThresholdMs = 3000;
    constexpr int VolumeSaveDelayMs = 500;

    double RoundedVolume(float volume)
    {
        return std::round(static_cast<double>(volume) * 100) / 100;
    }

    QString RepeatToString(PlayerSubsystem::RepeatMode mode)
    {
        switch (mode)
        {
            case PlayerSubsystem::RepeatMode::All: return QStringLiteral("all");
            case PlayerSubsystem::RepeatMode::One: return QStringLiteral("one");
            default:                               return QStringLiteral("off");
        }
    }

    PlayerSubsystem::RepeatMode RepeatFromString(const QString& text)
    {
        if (text == QStringLiteral("all"))
            return PlayerSubsystem::RepeatMode::All;
        if (text == QStringLiteral("one"))
            return PlayerSubsystem::RepeatMode::One;
        return PlayerSubsystem::RepeatMode::Off;
    }
}

PlayerSubsystem::PlayerSubsystem()
    : player(new QMediaPlayer(this))
    , audioOutput(new QAudioOutput(this))
    , devices(new QMediaDevices(this))
{
    player->setAudioOutput(audioOutput);

    volume = static_cast<float>(qBound(0.0, AppConfigs::GetDouble(AppConfigs::SettingsKeys::PlayerVolume, 0.8), 1.0));
    bShuffle = AppConfigs::GetBool(AppConfigs::SettingsKeys::PlayerShuffle);
    repeatMode = RepeatFromString(AppConfigs::GetString(AppConfigs::SettingsKeys::PlayerRepeat));
    SetVolume(volume);
    ApplyOutputDevice();

    volumeSaveTimer.setSingleShot(true);
    volumeSaveTimer.setInterval(VolumeSaveDelayMs);
    connect(&volumeSaveTimer, &QTimer::timeout, this, [this] {
        QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::PlayerVolume, RoundedVolume(volume));
    });

    connect(devices, &QMediaDevices::audioOutputsChanged, this, &PlayerSubsystem::ApplyOutputDevice);

    connect(player, &QMediaPlayer::playbackStateChanged, this, &PlayerSubsystem::playingChanged);
    connect(player, &QMediaPlayer::positionChanged, this, &PlayerSubsystem::positionChanged);
    connect(player, &QMediaPlayer::mediaStatusChanged, this,
        [this](QMediaPlayer::MediaStatus status) { OnMediaStatusChanged(status); });

    connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 durationMs) {
        LibrarySubsystem::Get().SetDuration(GetCurrentTrack(), durationMs);
        emit durationChanged(durationMs);
    });

    connect(player, &QMediaPlayer::errorOccurred, this,
        [this](QMediaPlayer::Error, const QString& message) { OnPlayerError(message); });
}

void PlayerSubsystem::Deinitialize()
{
    if (volumeSaveTimer.isActive())
    {
        volumeSaveTimer.stop();
        QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::PlayerVolume, RoundedVolume(volume));
    }

    player->stop();

    delete player;
    player = nullptr;
    delete audioOutput;
    audioOutput = nullptr;
    delete devices;
    devices = nullptr;
}

void PlayerSubsystem::ApplyOutputDevice()
{
    if (!audioOutput)
        return;

    const QByteArray wanted = AppConfigs::GetString(AppConfigs::SettingsKeys::OutputDevice).toUtf8();
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!wanted.isEmpty())
    {
        for (const QAudioDevice& output : QMediaDevices::audioOutputs())
        {
            if (output.id() == wanted)
                device = output;
        }
    }

    if (audioOutput->device() != device)
    {
        qDebug() << "[Player] Output:" << device.description();
        audioOutput->setDevice(device);
    }
}

TrackId PlayerSubsystem::GetCurrentTrack() const
{
    return currentIndex >= 0 && currentIndex < queue.size() ? queue[currentIndex] : TrackId{};
}

bool PlayerSubsystem::IsPlaying() const
{
    if (bLoading)
        return bPlayWhenLoaded;

    return player && player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 PlayerSubsystem::GetPosition() const
{
    return player ? player->position() : 0;
}

qint64 PlayerSubsystem::GetDuration() const
{
    if (player && player->duration() > 0)
        return player->duration();

    const Track* track = LibrarySubsystem::Get().FindTrack(GetCurrentTrack());
    return track ? track->tags.durationMs : 0;
}

void PlayerSubsystem::PlayQueue(const QList<TrackId>& newQueue, int index)
{
    if (index < 0 || index >= newQueue.size())
        return;

    queue = newQueue;
    PlayIndex(index);
}

void PlayerSubsystem::PlayIndex(int index)
{
    if (!player)
        return;

    const TrackId id = queue.value(index);
    const Track* track = LibrarySubsystem::Get().FindTrack(id);
    if (!track)
    {
        qWarning() << "[Player] Track is no longer in the library";
        return;
    }

    currentIndex = index;
    ++loadRequest;
    bStreamRetried = false;
    pendingSeekMs = 0;

    const bool bLocal = track->HasLocalFile() && QFileInfo::exists(track->file.path);

    if (!bLocal && track->IsOnline())
    {
        player->setSource(QUrl());
        bPlayWhenLoaded = true;
        SetLoading(true);
        LoadStream(track->stream.pageUrl, 0);
    }
    else if (bLocal)
    {
        SetLoading(false);
        player->setSource(QUrl::fromLocalFile(track->file.path));
        player->play();
    }
    else
    {
        SetLoading(false);
        player->setSource(QUrl());
        qWarning() << "[Player] The file is gone:" << track->file.path;
        emit errorOccurred(QStringLiteral("The file is gone: ") + track->file.path);
    }

    LibrarySubsystem::Get().MarkPlayed(id);
    emit currentTrackChanged();
}

void PlayerSubsystem::LoadStream(const QString& pageUrl, qint64 startMs)
{
    const quint64 request = loadRequest;

    OnlineSubsystem::Get().ResolveStream(pageUrl, this, [this, request, startMs](const QUrl& stream, const QString& error) {
        if (request != loadRequest || !player)
            return;

        if (stream.isEmpty())
        {
            SetLoading(false);
            qWarning() << "[Player] Cannot play the online track:" << error;
            emit errorOccurred(error);
            return;
        }

        pendingSeekMs = startMs;
        player->setSource(stream);
        if (bPlayWhenLoaded)
            player->play();
        SetLoading(false);
    });
}

void PlayerSubsystem::SetLoading(bool bValue)
{
    if (bLoading == bValue)
        return;

    bLoading = bValue;
    emit loadingChanged();
    emit playingChanged();
}

void PlayerSubsystem::OnPlayerError(const QString& message)
{
    const Track* track = LibrarySubsystem::Get().FindTrack(GetCurrentTrack());
    const bool bStreaming = track && track->IsOnline() && !(track->HasLocalFile() && QFileInfo::exists(track->file.path));
    if (bStreaming && !bStreamRetried)
    {
        qWarning() << "[Player]" << message << "- resolving the stream again";
        bStreamRetried = true;
        ++loadRequest;

        const qint64 position = player->position();
        OnlineSubsystem::Get().ReportStreamFailed(track->stream.pageUrl);
        bPlayWhenLoaded = true;
        SetLoading(true);
        LoadStream(track->stream.pageUrl, position);
        return;
    }

    qWarning() << "[Player]" << message;
    emit errorOccurred(message);
}

void PlayerSubsystem::Play()
{
    if (!player || !GetCurrentTrack().IsValid())
        return;

    if (bLoading)
    {
        bPlayWhenLoaded = true;
        emit playingChanged();
        return;
    }
    player->play();
}

void PlayerSubsystem::Pause()
{
    if (!player)
        return;

    if (bLoading)
    {
        bPlayWhenLoaded = false;
        emit playingChanged();
        return;
    }
    player->pause();
}

void PlayerSubsystem::TogglePlay()
{
    if (IsPlaying())
        Pause();
    else
        Play();
}

void PlayerSubsystem::Next()
{
    if (queue.isEmpty() || !player)
        return;

    if (bShuffle && queue.size() > 1)
    {
        int index = currentIndex;
        while (index == currentIndex)
            index = QRandomGenerator::global()->bounded(static_cast<int>(queue.size()));
        PlayIndex(index);
    }
    else if (currentIndex + 1 < queue.size())
    {
        PlayIndex(currentIndex + 1);
    }
    else if (repeatMode != RepeatMode::Off)
    {
        PlayIndex(0);
    }
    else
    {
        StopAtEnd();
    }
}

void PlayerSubsystem::StopAtEnd()
{
    player->stop();
    emit playingChanged();
    emit positionChanged(0);
}

void PlayerSubsystem::Previous()
{
    if (!player || !GetCurrentTrack().IsValid())
        return;

    if (player->position() > RestartThresholdMs || currentIndex == 0)
        player->setPosition(0);
    else
        PlayIndex(currentIndex - 1);
}

void PlayerSubsystem::Seek(qint64 positionMs)
{
    if (player)
        player->setPosition(positionMs);
}

void PlayerSubsystem::SetVolume(float value)
{
    volume = qBound(0.0f, value, 1.0f);

    if (audioOutput)
    {
        audioOutput->setVolume(
            QtAudio::convertVolume(volume, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale));
    }
    volumeSaveTimer.start();
    emit volumeChanged();
}

void PlayerSubsystem::SetShuffle(bool bValue)
{
    if (bShuffle == bValue)
        return;

    bShuffle = bValue;
    QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::PlayerShuffle, bShuffle);
    emit shuffleChanged();
}

void PlayerSubsystem::SetRepeatMode(RepeatMode mode)
{
    if (repeatMode == mode)
        return;

    repeatMode = mode;
    QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::PlayerRepeat, RepeatToString(repeatMode));
    emit repeatChanged();
}

void PlayerSubsystem::OnMediaStatusChanged(int status)
{
    if (status == QMediaPlayer::EndOfMedia)
    {
        if (repeatMode == RepeatMode::One)
        {
            player->setPosition(0);
            player->play();
            return;
        }
        Next();
        return;
    }

    if (pendingSeekMs > 0 && (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia))
    {
        player->setPosition(pendingSeekMs);
        pendingSeekMs = 0;
    }
}
