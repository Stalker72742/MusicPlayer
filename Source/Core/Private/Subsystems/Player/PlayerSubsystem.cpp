//
// Created by Stalker7274 on 24.03.2025.
//

#include "PlayerSubsystem.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QDirIterator>
#include <QJsonDocument>
#include <AppInstance.h>
#ifdef Q_OS_ANDROID
#include <QtCore/private/qandroidextras_p.h>
#endif
#include <QDebug>
#include <QFileInfo>
#include <QJsonObject>
#include "../../../Public/Subsystems/Player/SubObjects/song.h"
#include "PlayerBackend.h"
#include "SubObjects/playlist.h"

PlayerSubsystem::PlayerSubsystem(::playerBackend* Backend, QObject *parent) {

    Parent = parent;
    playerBackend = Backend;

    updateTimer = new QTimer(this);
    updateTimer->setInterval(1000);

    SetVolume(50);

    connect(updateTimer, &QTimer::timeout, this, &PlayerSubsystem::updateSliderPosition);

    connect(playerBackend, &playerBackend::onPlayerStateChanged, this, &PlayerSubsystem::onBackendStateChanged);

#ifdef Q_OS_ANDROID

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

    SetVolume(100);
#endif

    currentPlaylistPtr = playlist::constructDir(DefaultMusicFolder);
    playerBackend->setSource(currentPlaylistPtr->getSongs()[0]->getSongPath());
}
PlayerSubsystem::~PlayerSubsystem() {

    playerBackend->deleteLater();
}

void PlayerSubsystem::initJavaPlayer() {

}

void PlayerSubsystem::registerJavaCallbacks() {

}

void PlayerSubsystem::updateMediaSessionState(const QString &state)
{
    /*if (mediaSessionHandler.isValid()) {
        if (state == "PLAYING") {
            mediaSessionHandler.callMethod<void>("setPlaying");
        } else if (state == "PAUSED") {
            mediaSessionHandler.callMethod<void>("setPaused");
        } else if (state == "STOPPED") {
            mediaSessionHandler.callMethod<void>("setStopped");
        }
    }*/
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

    song* currentSong = currentPlaylistPtr->getCurrentSong();

    if (playerBackend) {
        QString songPath = currentSong->getSongPath();

        qDebug() << "Playing song via Java backend: " << songPath;
        
        playerBackend->setSource(songPath);
        playerBackend->play();

        emit playingSongChanged(currentSong);
    } else {
        qDebug() << "Java player not valid, trying to get audio stream";

        if (!currentSong->getSongPath().startsWith("/")) {
            //connect(currentSong, &song::audioStreamLoaded, this, &PlayerSubsystem::onAudioStreamLoaded);
        }
    }
}

void PlayerSubsystem::Resume() {

    if (playerBackend) {
        playerBackend->unpause();
        bPaused = false;
    } else {
        PlayCurrentSong();
    }
}

void PlayerSubsystem::Pause() const {

    if (playerBackend) {
        playerBackend->pause();
    }
}

void PlayerSubsystem::SetVolume(const int volume) {
    currentVolume = volume;
    if (playerBackend) {
        playerBackend->setVolume(static_cast<float>(volume));
    }
}

int PlayerSubsystem::getVolume() const {
    return currentVolume;
}

qint64 PlayerSubsystem::getMaxDuration() const {
    /*if (javaPlayer.isValid()) {
        return javaPlayer.callMethod<jlong>("getDuration");
    }*/
    return currentDuration;
}

void PlayerSubsystem::NextSong() {

    currentPlaylistPtr->next();

    qDebug() << "Start playing next song";

    PlayCurrentSong();
}

void PlayerSubsystem::PreviousSong() {
    CurrentSongIndex--;

    if (CurrentSongIndex < 0) {
        CurrentSongIndex = getSongs().size() - 1;
    }

    PlayCurrentSong();
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
void PlayerSubsystem::onBackendStateChanged(EPlayerState inBackedState)
{
    switch (inBackedState)
    {
    case EPlayerState::Loading:
        break;
    case EPlayerState::Playing:
        break;
    case EPlayerState::Stopped:
        break;
    case EPlayerState::Finished:
        playerBackend->setSource(currentPlaylistPtr->next()->getSongPath());
        break;
    case EPlayerState::Error:
        break;
    }
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

    if(QDir(playlistPathLocal).exists()){

        currentPlaylist.clear();

        playlistPath = playlistPathLocal;
        QDirIterator it(playlistPath, {"*.mp3"}, QDir::Files, QDirIterator::Subdirectories);

        while(it.hasNext()){
            QString songPath = it.next();

            currentPlaylist.append(new song(songPath));

            currentPlaylist.last()->setName(QFileInfo(songPath).baseName());

            qDebug() << "Loaded song: " << currentPlaylist.last()->getName();
        }

        emit playlistChanged();

        if (!currentPlaylist.empty()) {

            CurrentSongIndex = 0;

        }else {

            qDebug() << "No songs found";
        }
    }else{

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
}

void PlayerSubsystem::startPlayFromIndex(int index){
    CurrentSongIndex = index;
    PlayCurrentSong();
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

void PlayerSubsystem::playPause(){

    if(getSongs().empty()) return;

    if(bPaused){
        if (playerBackend) {
            playerBackend->play();
            bPaused = false;
        } else {
            PlayCurrentSong();
        }
    }else{
        if (playerBackend) {
            playerBackend->pause();
            bPaused = true;
        }
    }
}

// Static callbacks for Java
/*void PlayerSubsystem::onJavaPlaybackStateChanged(JNIEnv *env, jobject obj, jint state) {
    PlayerSubsystem* player = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>();
    if (player) {
        switch (state) {
        case 1: // PLAYING
            player->updateTimer->start();
            player->bPaused = false;
            player->updateMediaSessionState("PLAYING");
            qDebug() << "Java Player: PLAYING";
            break;
        case 2: // PAUSED
            player->bPaused = true;
            player->updateMediaSessionState("PAUSED");
            qDebug() << "Java Player: PAUSED";
            break;
        case 3: // STOPPED
            player->updateTimer->stop();
            player->bPaused = true;
            player->time = 0;
            player->updateMediaSessionState("STOPPED");
            emit player->updateMusicDuration(0);
            qDebug() << "Java Player: STOPPED";
            break;
        }
    }
}

void PlayerSubsystem::onJavaSongFinished(JNIEnv *env, jobject obj) {
    PlayerSubsystem* player = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>();
    if (player) {
        qDebug() << "Java Player: Song finished, playing next";
        player->NextSong();
    }
}

void PlayerSubsystem::onJavaError(JNIEnv *env, jobject obj, jstring error) {
    QString errorStr = QJniObject(error).toString();
    qDebug() << "Java Player Error:" << errorStr;
}

// Остальные методы без изменений
extern "C" JNIEXPORT void JNICALL
Java_com_example_MusicPlayer_MediaSessionHandler_onMediaButton(JNIEnv *env, jobject obj,
                                                               jint keyCode, jstring action)
{
    QString actionStr = QJniObject(action).toString();
    qDebug() << "🎧 Получена медиа кнопка:" << keyCode << actionStr;

    PlayerSubsystem* player = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>();

    if (player) {
        switch (keyCode) {
        case 127:
            player->Pause();
            break;
        case 126:
            player->Resume();
            break;
        case 87:
            player->NextSong();
            break;
        }
    }
}*/
