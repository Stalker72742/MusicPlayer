//
// Created by Stalker7274 on 24.03.2025.
//

#include "PlayerSubsystem.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QDirIterator>
#include <QJsonDocument>
#include <QtCore/private/qandroidextras_p.h>
#include <QDebug>
#include "song.h"
#include "songPath.h"

PlayerSubsystem::PlayerSubsystem(QObject *parent) {

    Parent = parent;

    player = new QMediaPlayer(this);

    audioOutput = new QAudioOutput;
    audioOutput->setDevice(QMediaDevices::defaultAudioOutput());

    updateTimer = new QTimer(this);
    updateTimer->setInterval(1000);

    SetVolume(50);

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

    connect(updateTimer, &QTimer::timeout, this, &PlayerSubsystem::updateSliderPosition);

    connect(player, &QMediaPlayer::playbackStateChanged,
            [this](QMediaPlayer::PlaybackState state) {
        switch (state) {
        case QMediaPlayer::PlayingState:

            updateTimer->start();
            break;
        case QMediaPlayer::PausedState:

            break;
        case QMediaPlayer::StoppedState:

            updateTimer->stop();
            emit updateMusicDuration(0);
            time = 0;

            break;

        default:

            break;
        }
    });

#if QT_ANDROID

    QJniObject environment = QJniObject::callStaticObjectMethod(
        "android/os/Environment",
        "getExternalStoragePublicDirectory",
        "(Ljava/lang/String;)Ljava/io/File;",
        QJniObject::getStaticObjectField(
            "android/os/Environment",
            "DIRECTORY_MUSIC",
            "Ljava/lang/String;"
        ).object()
    );

    DefaultMusicFolder = environment.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString();
    DefaultMediaLibFolder = DefaultMusicFolder + "/MediaLib";

    //setCurrentPlaylist(getPlaylists()[0]);

#endif
}

PlayerSubsystem::~PlayerSubsystem() {

    player->stop();

    delete audioOutput;
    delete player;
}

void PlayerSubsystem::checkMusicFolder() {

    QFile allSongs(DefaultMediaLibFolder + "/AllSongs.json");

    if (allSongs.exists()) { return; }
    QDir(DefaultMediaLibFolder).mkdir(DefaultMediaLibFolder);

    if (!allSongs.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Fail to open file: " << DefaultMediaLibFolder + "/AllSongs.json" << " Error:" << allSongs.errorString();
        return;
    }

    QJsonObject songs;

    QDirIterator it(DefaultMusicFolder, QStringList{".mp3", ".mp4"}, QDir::Files,
        QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString songPath = it.next();
        songs[it.fileName()] = songPath;
    }

    const QJsonDocument doc(songs);
    allSongs.write(doc.toJson(QJsonDocument::Indented));
    allSongs.close();

    qDebug() << "Successfully saved file: " << DefaultMediaLibFolder + "/AllSongs.json";
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

    QUrl song = getSongs()[CurrentSongIndex]->getPlayerUrl();

    if (song.isValid()) {

        player->setSource(song);
        player->play();
    }else {

        connect(getSongs()[CurrentSongIndex], &song::audioStreamLoaded, this, &PlayerSubsystem::onAudioStreamLoaded);
    }
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

void PlayerSubsystem::SetVolume(const int volume) const {
    audioOutput->setVolume(static_cast<float>(volume) / 100);
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

QList<QString> PlayerSubsystem::getLocalSongsPaths() const {

    QList<QString> songs;

    const QDir dir(DefaultMusicFolder);
    QStringList filters;
    filters << "*.mp3" << "*.wav" << "*.mp4";

    QDirIterator it(dir.absolutePath(), filters, QDir::Files);

    while (it.hasNext()) {
        it.next();
        QString filePath = it.filePath();

        songs.append(filePath);
    }

    return songs;
}

void PlayerSubsystem::showMediaLib() {

    emit onShowMediaLib(getLocalSongsPaths());
}

void PlayerSubsystem::PlayerError(QMediaPlayer::Error Error, const QString &error) {
    qDebug() << "Player: " << Error + " " + error;
}

void PlayerSubsystem::onAudioStreamLoaded(song *songPtr) {

    disconnect(songPtr, &song::audioStreamLoaded, this, &PlayerSubsystem::onAudioStreamLoaded);

    PlayCurrentSong();
}

void PlayerSubsystem::updateSliderPosition() {

    time++;

    emit updateMusicDuration(time);
}

QList<song*> PlayerSubsystem::getSongs() {
    return bUseQueue ? queueSongs : currentPlaylist;
}

QString PlayerSubsystem::createPlaylist(QString playlistName) {

    QFile file(DefaultMediaLibFolder + "/" + playlistName + ".json");

    QFileInfo fileInfo(file);
    QDir dir = fileInfo.absoluteDir();

    bool bSuccess = true;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Fail to save file:" << file.errorString();

        bSuccess = false;
    }

    return bSuccess ? DefaultMediaLibFolder + "/" + playlistName + ".json" : "";
}

void PlayerSubsystem::setCurrentPlaylist(QString playlistPathLocal) {

    QFile configFile(playlistPathLocal);
    if (!configFile.open(QIODevice::ReadOnly)) return;

    currentPlaylist.clear();

    playlistPath = playlistPathLocal;

    QJsonObject json = QJsonDocument::fromJson(configFile.readAll()).object();

    foreach(QString key, json.keys()) {

        currentPlaylist.append(new song(json[key].toString()));

        currentPlaylist.last()->setName(key);

        qDebug() << "Loaded song: " << key;
    }

    emit playlistChanged();

    if (!currentPlaylist.empty()) {

        CurrentSongIndex = 0;

    }else {

        qDebug() << "No songs found";
    }

}

void PlayerSubsystem::removePlaylist(QString playlistPath) {

}

QList<QString> PlayerSubsystem::getPlaylists() {

    QList<QString> playlists;

    const QDir dir(DefaultMediaLibFolder);
    QStringList filters;
    filters << "*.json";

    QDirIterator it(dir.absolutePath(), filters, QDir::Files);
    int index = 0;

    while (it.hasNext()) {
        it.next();
        QString filePath = it.filePath();

        QFile configFile(filePath);
        if (!configFile.open(QIODevice::ReadOnly)) return playlists;

        playlists.append(filePath);

        index++;
    }

    return playlists;
}

void PlayerSubsystem::addSongToPlaylistByName(QString Song, QString playlistName) {

    QList<QString> playlists = getPlaylists();

    QString playlist;

    foreach(QString x, playlists) {

        if (x.contains(playlistName)) {

            playlist = playlistName;

            break;
        }
    }

    QFile configFile(playlist);
    if (!configFile.open(QIODevice::ReadOnly)) return;

    QJsonObject playlistJson = QJsonDocument::fromJson(configFile.readAll()).object();

    playlistJson[Song.split('.').first().split('/').last()] = Song.split("Music").last();

    QFile file(playlist);

    QFileInfo fileInfo(file);
    QDir dir = fileInfo.absoluteDir();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Fail to save file:" << file.errorString();
        return;
    }

    const QJsonDocument doc(playlistJson);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

}

template<typename P>
void PlayerSubsystem::addSongToPlaylist(song *Song, P playlist) {



}

void PlayerSubsystem::addSongToQueue(song* Song) {

    qDebug() << "Adding song to queue: " << Song->getName();

    queueSongs.append(Song);
}
