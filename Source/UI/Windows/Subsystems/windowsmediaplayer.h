#ifndef WINDOWSMEDIAPLAYER_H
#define WINDOWSMEDIAPLAYER_H

#pragma once
#include "../../../Core/Public/Subsystems/Player/PlayerBackend.h"

#include <QMediaPlayer>
#include <QAudioOutput>

class WindowsMediaPlayer : public playerBackend {
    Q_OBJECT

public:
    explicit WindowsMediaPlayer(QObject* parent);
    ~WindowsMediaPlayer() override;

    void setSource(const QString& source) override;
    void play() override;
    void pause() override;
    void unpause() override;
    void playPause() override;

    void setVolume(float volume) override;
    void setPosition(qint64 position) override;

private slots:
    void updatePosition() override;
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onDurationChanged(qint64 duration);
    void onPositionChanged(qint64 position);

private:
    QMediaPlayer* mediaPlayer;
    QAudioOutput* audioOutput;
};

#endif // WINDOWSMEDIAPLAYER_H
