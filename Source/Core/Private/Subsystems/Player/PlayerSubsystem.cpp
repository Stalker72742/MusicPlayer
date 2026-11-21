//
// Created by Stalker7274 on 03.10.2026.
//

#include "Player/PlayerSubsystem.h"

#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#include <QTomlUtils/QTomlUtils.h>

#include <cmath>
#include <utility>

#include "AppConfigs.h"
#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "Online/OnlineSubsystem.h"
#include "FfmpegBackend.h"
#include "Player/AudioDevices.h"
#include "StreamCapture.h"

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

    /// The volume slider is perceptual, samples are multiplied by a linear gain.
    float PerceptualToLinear(float value)
    {
        constexpr double Log100 = 4.60517018599;
        return value >= 0.99f ? 1.0f : static_cast<float>(-std::log(1.0 - value) / Log100);
    }

    QString ExtensionOfStream(const QUrl& stream)
    {
        const QString mime = QUrlQuery(stream).queryItemValue(QStringLiteral("mime"), QUrl::FullyDecoded);
        if (mime.contains(QStringLiteral("webm")))
            return QStringLiteral("webm");
        return QStringLiteral("m4a");
    }
}

PlayerSubsystem::PlayerSubsystem()
    : player(new FfmpegBackend(this))
    , devices(new AudioDevices(this))
    , capture(std::make_unique<StreamCapture>())
{
    ConnectBackend();

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

    connect(devices, &AudioDevices::changed, this, &PlayerSubsystem::ApplyOutputDevice);
}

PlayerSubsystem::~PlayerSubsystem() = default;

void PlayerSubsystem::ConnectBackend()
{
    connect(player, &AudioBackend::stateChanged, this, &PlayerSubsystem::playingChanged);
    connect(player, &AudioBackend::positionChanged, this, &PlayerSubsystem::positionChanged);
    connect(player, &AudioBackend::finished, this, &PlayerSubsystem::OnBackendFinished);
    connect(player, &AudioBackend::loaded, this, &PlayerSubsystem::OnBackendLoaded);
    connect(player, &AudioBackend::errorOccurred, this, &PlayerSubsystem::OnPlayerError);
    connect(player, &AudioBackend::sourceDataReceived, this, &PlayerSubsystem::OnSourceData);
    connect(player, &AudioBackend::sourceSizeKnown, this, [this](qint64 size) { capture->SetSize(size); });
    connect(player, &AudioBackend::durationChanged, this, [this](qint64 durationMs) {
        if (durationMs > 0)
            LibrarySubsystem::Get().SetDuration(GetCurrentTrack(), durationMs);
        emit durationChanged(durationMs);
    });
}

void PlayerSubsystem::Deinitialize()
{
    if (volumeSaveTimer.isActive())
    {
        volumeSaveTimer.stop();
        QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::PlayerVolume, RoundedVolume(volume));
    }

    delete player;
    player = nullptr;
    capture->Abort();
    delete devices;
    devices = nullptr;
}

void PlayerSubsystem::ApplyOutputDevice()
{
    if (player)
        player->SetOutputDevice(AppConfigs::GetString(AppConfigs::SettingsKeys::OutputDevice).toUtf8());
}

TrackId PlayerSubsystem::GetCurrentTrack() const
{
    return currentIndex >= 0 && currentIndex < queue.size() ? queue[currentIndex] : TrackId{};
}

bool PlayerSubsystem::IsPlaying() const
{
    if (bLoading)
        return bPlayWhenLoaded;

    return player && player->GetState() == AudioBackend::State::Playing;
}

qint64 PlayerSubsystem::GetPosition() const
{
    return player ? player->GetPosition() : 0;
}

qint64 PlayerSubsystem::GetDuration() const
{
    if (player && player->GetDuration() > 0)
        return player->GetDuration();

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
    capture->Abort();
    captureTrack = {};

    const bool bLocal = track->HasLocalFile() && QFileInfo::exists(track->file.path);

    if (!bLocal && track->IsOnline())
    {
        player->SetSource(QUrl());
        bPlayWhenLoaded = true;
        SetLoading(true);
        LoadStream(track->stream.pageUrl, 0);
    }
    else if (bLocal)
    {
        SetLoading(false);
        player->SetSource(QUrl::fromLocalFile(track->file.path));
        player->Play();
    }
    else
    {
        SetLoading(false);
        player->SetSource(QUrl());
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
        if (const Track* track = LibrarySubsystem::Get().FindTrack(GetCurrentTrack()))
            StartCapture(*track, stream);
        player->SetSource(stream);
        if (bPlayWhenLoaded)
            player->Play();
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
        capture->Abort();
        captureTrack = {};

        const qint64 position = player->GetPosition();
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
    player->Play();
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
    player->Pause();
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
    player->Stop();
    emit playingChanged();
    emit positionChanged(0);
}

void PlayerSubsystem::Previous()
{
    if (!player || !GetCurrentTrack().IsValid())
        return;

    if (player->GetPosition() > RestartThresholdMs || currentIndex == 0)
        player->Seek(0);
    else
        PlayIndex(currentIndex - 1);
}

void PlayerSubsystem::Seek(qint64 positionMs)
{
    if (player)
        player->Seek(positionMs);
}

bool PlayerSubsystem::ReleaseFile(TrackId id)
{
    if (!player || !id.IsValid() || GetCurrentTrack() != id || bLoading)
        return false;

    const Track* track = LibrarySubsystem::Get().FindTrack(id);
    if (!track || !track->HasLocalFile() || !QFileInfo::exists(track->file.path))
        return false;

    releasedTrack = id;
    releasedPositionMs = player->GetPosition();
    bReleasedPlaying = IsPlaying();
    ++loadRequest;
    player->SetSource(QUrl());
    return true;
}

void PlayerSubsystem::RestoreFile()
{
    const TrackId id = std::exchange(releasedTrack, TrackId{});
    if (!player || !id.IsValid() || GetCurrentTrack() != id)
        return;

    const Track* track = LibrarySubsystem::Get().FindTrack(id);
    if (!track)
        return;

    if (track->HasLocalFile() && QFileInfo::exists(track->file.path))
    {
        pendingSeekMs = releasedPositionMs;
        player->SetSource(QUrl::fromLocalFile(track->file.path));
        if (bReleasedPlaying)
            player->Play();
    }
    else if (track->IsOnline())
    {
        bPlayWhenLoaded = bReleasedPlaying;
        SetLoading(true);
        LoadStream(track->stream.pageUrl, releasedPositionMs);
    }
    else if (bReleasedPlaying)
    {
        Next();
        return;
    }

    emit playingChanged();
}

void PlayerSubsystem::SetVolume(float value)
{
    volume = qBound(0.0f, value, 1.0f);
    ApplyVolume();
    volumeSaveTimer.start();
    emit volumeChanged();
}

void PlayerSubsystem::ApplyVolume()
{
    if (player)
        player->SetVolume(PerceptualToLinear(volume));
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

void PlayerSubsystem::OnBackendFinished()
{
    if (repeatMode == RepeatMode::One)
    {
        player->Seek(0);
        player->Play();
        return;
    }
    Next();
}

void PlayerSubsystem::OnBackendLoaded()
{
    if (pendingSeekMs > 0)
    {
        player->Seek(pendingSeekMs);
        pendingSeekMs = 0;
    }
}

void PlayerSubsystem::StartCapture(const Track& track, const QUrl& stream)
{
    capture->Abort();
    captureTrack = {};

    const bool bWanted = AppConfigs::GetBool(AppConfigs::SettingsKeys::SaveWhileListening)
                      && player->GetCapabilities().testFlag(AudioBackend::SourceData);
    player->SetSourceDataEnabled(bWanted);
    if (!bWanted)
        return;

    captureExtension = ExtensionOfStream(stream);
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                       + QStringLiteral("/stream-capture/%1.part").arg(track.id.value);
    if (capture->Start(path))
        captureTrack = track.id;
}

void PlayerSubsystem::OnSourceData(qint64 offset, const QByteArray& bytes)
{
    if (!captureTrack.IsValid())
        return;

    capture->Add(offset, bytes);
    if (!capture->IsComplete())
        return;

    const TrackId track = captureTrack;
    captureTrack = {};
    DownloadSubsystem::Get().AdoptFile(track, capture->Take(), captureExtension);
}
