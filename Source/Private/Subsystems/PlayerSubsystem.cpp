//
// Created by Stalker7274 on 24.03.2025.
//

#include "PlayerSubsystem.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QDirIterator>
#include <QStringList>

PlayerSubsystem::PlayerSubsystem(QObject *parent) {

    Parent = parent;

    player = new QMediaPlayer(this);

    audioOutput = new QAudioOutput;
    audioOutput->setDevice(QMediaDevices::defaultAudioOutput());

    audioOutput->setVolume(0.2);

    player->setAudioOutput(audioOutput);

    connect(player, &QMediaPlayer::errorOccurred, this, &PlayerSubsystem::PlayerError);

    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        switch (status) {
            case QMediaPlayer::EndOfMedia:
                NextSong();
                break;
            default:
                qDebug() << "Media status changed: " << status;
        }
    });

    LoadSongs();

    //PlayCurrentSong();
}

PlayerSubsystem::~PlayerSubsystem() {

    player->stop();
    delete audioOutput;
    delete player;
}

void PlayerSubsystem::LoadSongs() {
    const QDir dir(QDir::currentPath() + "/Music");
    QStringList filters;
    filters << "*.mp3" << "*.mp4"<< "*.webm";

    QDirIterator it(dir.absolutePath(), filters, QDir::Files);
    int index = 0;

    while (it.hasNext()) {
        it.next();
        QString filePath = it.filePath();

        Songs.append(filePath);

        index++;

        qDebug() << "Loaded song: " << filePath.split('/').last();
    }

    if (!Songs.empty()) {

        CurrentSong = Songs[0];
        CurrentSongIndex = 0;

    }else {

        qDebug() << "No songs found";
    }
}

void PlayerSubsystem::PlayCurrentSong() const {
    player->setSource(QUrl::fromLocalFile(CurrentSong));
    player->play();
}

void PlayerSubsystem::Resume() const {

    if (player->source().isValid()) {

        player->play();

    }else {

        PlayCurrentSong();

    }
}

void PlayerSubsystem::Pause() const {
    player->pause();
}

void PlayerSubsystem::SetVolume(const float volume) const {
    audioOutput->setVolume(volume);
}

void PlayerSubsystem::NextSong() {
    CurrentSongIndex++;

    if (CurrentSongIndex >= Songs.size()) {
        CurrentSongIndex = 0;
    }
    player->setSource(QUrl::fromLocalFile(Songs[CurrentSongIndex]));
    player->play();
}

void PlayerSubsystem::PreviousSong() {
    CurrentSongIndex--;

    if (CurrentSongIndex <= 0) {
        CurrentSongIndex = Songs.size() - 1;
    }
    player->setSource(QUrl::fromLocalFile(Songs[CurrentSongIndex]));
    player->play();
}

void PlayerSubsystem::PlayerError(QMediaPlayer::Error Error, const QString &error) {
    qDebug() << "Player: " + error;
}
