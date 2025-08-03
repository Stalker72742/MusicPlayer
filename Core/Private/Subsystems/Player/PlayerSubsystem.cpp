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

    connect(player, &QMediaPlayer::errorChanged, this, [this]() {
        qDebug() << "Error:" << player->errorString();
    });

    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        switch (status) {
            case QMediaPlayer::EndOfMedia:
                NextSong();
            case QMediaPlayer::BufferedMedia:
                player->play();
            case QMediaPlayer::LoadedMedia:
                player->play();
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
            updateMediaSessionState("PLAYING");
            break;
        case QMediaPlayer::PausedState:
            updateMediaSessionState("PAUSED");
            break;
        case QMediaPlayer::StoppedState:

            updateTimer->stop();
            emit updateMusicDuration(0);
            time = 0;
            updateMediaSessionState("STOPPED");

            break;

        default:

            break;
        }

        qDebug() << "Playback state changed:" << state;
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

    for (const auto &device : QMediaDevices::audioOutputs()) {
        qDebug() << "Audio:" << device.description();
        if (device.description().contains("speaker", Qt::CaseInsensitive)) {
            //audioOutput->setDevice(device);
            break;
        }
    }

    SetVolume(100);

    QTimer::singleShot(100, [=]() {
        QJniObject activity = QJniObject::callStaticMethod<QJniObject>(
            "org/qtproject/qt/android/QtNative",
            "activity",
            "()Landroid/app/Activity;"
            );

        if (activity.isValid()) {
            mediaSessionHandler = QJniObject("com/example/MusicPlayer/MediaSessionHandler",
                                             "(Landroid/content/Context;)V",
                                             activity.object());

            if (mediaSessionHandler.isValid()) {
                qDebug() << "✅ MediaSessionHandler создан с задержкой";
                mediaSessionHandler.callMethod<void>("requestAudioFocus");
            }
        }
    });
#endif
}

PlayerSubsystem::~PlayerSubsystem() {

    player->stop();

    delete audioOutput;
    delete player;
}

void PlayerSubsystem::updateMediaSessionState(const QString &state)
{
    if (mediaSessionHandler.isValid()) {
        if (state == "PLAYING") {
            mediaSessionHandler.callMethod<void>("setPlaying");
        } else if (state == "PAUSED") {
            mediaSessionHandler.callMethod<void>("setPaused");
        } else if (state == "STOPPED") {
            mediaSessionHandler.callMethod<void>("setStopped");
        }
    }
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

    for (auto x : getSongs()){
        qDebug() << "songs: " << x->getPlayerUrl();
    }

    if (song.isValid()) {

        qDebug() << "Playing song: " << song.path();

        player->stop();
        player->setSource(song);
        emit playingSongChanged(getSongs()[CurrentSongIndex]);
    }else {

        qDebug() << "Song not valid waiting for audio Stream loaded";

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
        if(player->hasAudio()){

            player->play();
        }else{
            PlayCurrentSong();
        }
    }else{

        player->pause();
    }
}

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
}
