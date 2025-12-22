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
#include <QStandardPaths>
#include <QSlider>
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
    connect(playerBackend, &playerBackend::onUpdatePosition, this,
        [this](playerPosition pos) {
            emit onPositionChanged(pos.currentMs, pos.totalMs);
        });

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
#elifdef Q_OS_WIN

    // Get standard Windows Music folder
    DefaultMusicFolder = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    DefaultMediaLibFolder = DefaultMusicFolder + "/MediaLib";

    qDebug() << "Windows Music folder:" << DefaultMusicFolder;

    SetVolume(50);
#endif

    setCurrentPlaylist(playlist::constructDir(DefaultMusicFolder));
}

void PlayerSubsystem::SetIsPlaying(bool isPlaying)
{
    bIsPlaying = isPlaying;
    emit OnPlayingStateChanged(bIsPlaying);
}

PlayerSubsystem::~PlayerSubsystem() {

    playerBackend->deleteLater();
}

void PlayerSubsystem::initJavaPlayer() {

}

void PlayerSubsystem::registerJavaCallbacks() {

}

void PlayerSubsystem::SetSource(song* InSong)
{
    if (playerBackend && InSong)
    {
        playerBackend->setSource(InSong->getSongPath());
        emit OnSongChanged(InSong);
    }
}

void PlayerSubsystem::setCurrentPlaylist(playlist *InNewPlaylist)
{
    currentPlaylistPtr = InNewPlaylist;
    emit onPlaylistChanged(currentPlaylistPtr);

    if (currentPlaylistPtr)
    {
        if (currentPlaylistPtr->getSongs().size() > 0)
        {
            if (song* currentSong = currentPlaylistPtr->getSongs()[0])
            {
                SetSource(currentSong);
            }
        }
    }
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

    if (playerBackend && currentSong) {
        currentSong->getSongPath();

        qDebug() << "Playing song via Java backend: " << currentSong->getSongPath();
        
        SetSource(currentSong);
        playerBackend->play();

        SetIsPlaying(true);
    } /*else {
        qDebug() << "Java player not valid, trying to get audio stream";

        if (!currentSong->getSongPath().startsWith("/")) {
            //connect(currentSong, &song::audioStreamLoaded, this, &PlayerSubsystem::onAudioStreamLoaded);
        }
    }*/
}

void PlayerSubsystem::Resume() {

    if (playerBackend) {
        playerBackend->unpause();
        bPaused = false;
    } else {
        PlayCurrentSong();
    }
}

void PlayerSubsystem::Pause() {

    if (playerBackend) {
        playerBackend->pause();
        SetIsPlaying(false);
    }
}

void PlayerSubsystem::SetVolume(const int volume) {
    if (currentVolume != volume) {
        currentVolume = volume;
        if (playerBackend) {
            playerBackend->setVolume(static_cast<float>(volume));
        }
        emit onVolumeChanged(volume);
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
    currentPlaylistPtr->prev();

    qDebug() << "Start playing previous song";

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

        SetSource(currentPlaylistPtr->next());

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

    if (playerBackend)
    {
        playerBackend->playPause();
        SetIsPlaying(!bIsPlaying);
    }
}

void PlayerSubsystem::bindPositionSlider(QSlider* slider)
{
    if (!slider) return;

    // Setup slider range (0-100%)
    slider->setRange(0, 100);
    slider->setValue(0);

    // Track if music was playing before dragging
    bool* wasPlaying = new bool(false);

    // Update slider when position changes (but not while dragging)
    connect(this, &PlayerSubsystem::onPositionChanged, slider,
        [slider](qint64 currentMs, qint64 totalMs) {
            if (totalMs > 0 && !slider->isSliderDown()) {
                int percentage = static_cast<int>((currentMs * 100) / totalMs);
                slider->setValue(percentage);
            }
        });

    // Pause playback when user starts dragging
    connect(slider, &QSlider::sliderPressed, this, [this, wasPlaying]() {
        *wasPlaying = bIsPlaying;
        if (bIsPlaying) {
            Pause();
            qDebug() << "Paused for seeking";
        }
    });

    // Seek and resume when user releases slider
    connect(slider, &QSlider::sliderReleased, this, [this, slider, wasPlaying]() {
        if (playerBackend) {
            qint64 totalMs = playerBackend->getPosition().totalMs;
            if (totalMs > 0) {
                qint64 newPosition = (slider->value() * totalMs) / 100;
                playerBackend->setPosition(newPosition);
                qDebug() << "Seeked to:" << newPosition << "ms";
            }
        }

        // Resume if it was playing before
        if (*wasPlaying) {
            Resume();
            qDebug() << "Resumed after seeking";
        }
    });

    qDebug() << "Position slider bound successfully";
}

void PlayerSubsystem::bindVolumeSlider(QSlider* slider)
{
    if (!slider) return;

    // Setup slider range (0-100)
    slider->setRange(0, 100);
    slider->setValue(currentVolume);

    // Update volume when slider changes
    connect(slider, &QSlider::valueChanged, this, &PlayerSubsystem::SetVolume);

    // Update slider when volume changes programmatically
    connect(this, &PlayerSubsystem::onVolumeChanged, slider, &QSlider::setValue);

    qDebug() << "Volume slider bound successfully";
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
}*/
