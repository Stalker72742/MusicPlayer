//
// Created by Stalker7274 on 24.03.2025.
//

#include "PlayerSubsystem.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QDirIterator>
#include <QJsonDocument>

#include "song.h"
#include "songPath.h"

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

    bUseQueue = false;
}

PlayerSubsystem::~PlayerSubsystem() {

    player->stop();

    delete audioOutput;
    delete player;
}

void PlayerSubsystem::savePlaylist(){

    QJsonObject playlist;

    foreach(song* Song, currentPlaylist) {

        playlist[Song->getName()] = Song->getSongPath();
    }

    QFile file(DefaultMediaLibFolder + "/" + "playlist" + ".json");

    QFileInfo fileInfo(file);
    QDir dir = fileInfo.absoluteDir();

    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Unable to make directory:" << dir.path();
            return;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Fail to save file:" << file.errorString();
        return;
    }

    const QJsonDocument doc(playlist);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void PlayerSubsystem::LoadSongs() {

    const QDir dir(DefaultMediaLibFolder);
    QStringList filters;
    filters << "*.json";

    QDirIterator it(dir.absolutePath(), filters, QDir::Files);
    int index = 0;

    while (it.hasNext()) {
        it.next();
        QString filePath = it.filePath();

        QFile configFile(filePath);
        if (!configFile.open(QIODevice::ReadOnly)) return;

        QJsonObject json = QJsonDocument::fromJson(configFile.readAll()).object();

        foreach(QString key, json.keys()) {

            currentPlaylist.append(new song(json[key].toString()));

            currentPlaylist.last()->setName(key);

            qDebug() << "Loaded song: " << key;
        }

        index++;
    }

    if (!currentPlaylist.empty()) {

        CurrentSongIndex = 0;

    }else {

        qDebug() << "No songs found";
    }
}

void PlayerSubsystem::PlayCurrentSong() {

    player->setSource(getSongs()[CurrentSongIndex]->getPlayerUrl());
    player->play();
}

void PlayerSubsystem::Resume() {

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

    if (CurrentSongIndex >= getSongs().size()) {
        CurrentSongIndex = 0;
    }
    qDebug() << "Trying to play: " << getSongs()[CurrentSongIndex]->getPlayerUrl();
    player->setSource(getSongs()[CurrentSongIndex]->getPlayerUrl());
    player->play();
}

void PlayerSubsystem::PreviousSong() {
    CurrentSongIndex--;

    if (CurrentSongIndex <= 0) {
        CurrentSongIndex = getSongs().size() - 1;
    }
    player->setSource(getSongs()[CurrentSongIndex]->getPlayerUrl());
    player->play();
}

void PlayerSubsystem::addSong(song* Song) {

    currentPlaylist.append(Song);

    emit playlistUpdated();

    savePlaylist();
}

void PlayerSubsystem::PlayerError(QMediaPlayer::Error Error, const QString &error) {
    qDebug() << "Player: " + error;
}

QList<song*> PlayerSubsystem::getSongs() {
    return bUseQueue ? queueSongs : currentPlaylist;
}

void PlayerSubsystem::createPlaylist(QString playlistName) {

    QFile file(DefaultMediaLibFolder + "/" + playlistName + ".json");

    QFileInfo fileInfo(file);
    QDir dir = fileInfo.absoluteDir();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Fail to save file:" << file.errorString();
    }
}

void PlayerSubsystem::setCurrentPlaylist(QString playlistPath) {

}

void PlayerSubsystem::removePlaylist(QString playlistPath) {

}

template<typename P>
void PlayerSubsystem::addSongToPlaylist(song *Song, P playlist) {

}

void PlayerSubsystem::addSongToQueue(song* Song) {

    qDebug() << "Adding song to queue: " << Song->getName();

    queueSongs.append(Song);
}
