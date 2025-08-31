//
// Created by Stalker7274 on 24.03.2025.
//

#ifndef PLAYERSUBSYSTEM_H
#define PLAYERSUBSYSTEM_H

#include <QAudioOutput>
#include <QObject>
#include <QDir>
#include <QMediaPlayer>
#include <QTimer>
#include <QJniObject>
#include "PlayerBackend/playerbase.h"

class QSlider;
class song;

class PlayerSubsystem : public QObject {
    Q_OBJECT
public:

    explicit PlayerSubsystem(QObject* parent = nullptr);

    ~PlayerSubsystem() override;

    void LoadSongs();

    void PlayCurrentSong();

    void Resume();

    void Pause() const;

    void SetVolume(int volume);

    int getVolume() const;

    void NextSong();

    void PreviousSong();

    void addSong(song* Song);

    void addSongToQueue(song* Song);

    void savePlaylist();

    QList<song*> getSongs();

    QString createPlaylist(QString playlistName);

    void setCurrentPlaylist(QString playlistPathLocal);

    void startPlayFromIndex(int index);

    void removePlaylist(QString playlistPath);

    QList<QString> getPlaylists();

    template <typename P>
    void addSongToPlaylist(song* Song, P playlist);

    void addSongToPlaylistByName(QString Song, QString playlistName);

    qint64 getMaxDuration() const;

    QList<QString> getLocalSongsPaths() const;

    void showMediaLib();

    QString getMusicFolder() { return DefaultMusicFolder; }

    void playPause();

public slots:

    void PlayerError(QMediaPlayer::Error Error, const QString &error);

    void onAudioStreamLoaded(song* songPtr);

    void updateSliderPosition();

signals:

    void playlistUpdated();

    void playlistChanged();

    void updateMusicDuration(int newDuration);

    void onShowMediaLib(QList<QString> songs);

    void playingSongChanged(song* currentPlayingSong);

private:

    void checkMusicFolder();

    void updateMediaSessionState(const QString &state);

    // Java backend methods
    void initJavaPlayer();
    void registerJavaCallbacks();

    // Static callbacks for Java
    static void onJavaPlaybackStateChanged(JNIEnv *env, jobject obj, jint state);
    static void onJavaSongFinished(JNIEnv *env, jobject obj);
    static void onJavaError(JNIEnv *env, jobject obj, jstring error);

    // Убираем QMediaPlayer и QAudioOutput
    // QAudioOutput *audioOutput;  // УДАЛЕНО
    // QMediaPlayer *player;        // УДАЛЕНО

    // Добавляем Java плеер
    QJniObject javaPlayer;

    playerBase* customPlayer;

    QObject* Parent;

    QTimer* updateTimer;
    int time = 0;

    QString DefaultMusicFolder = QDir::currentPath() + "/Music";
    QString DefaultMediaLibFolder = DefaultMusicFolder + "/MediaLib";

    QList<song*> currentPlaylist;

    QString playlistPath;

    qint32 CurrentSongIndex;

    bool bUseQueue = false;

    QList<song*> queueSongs;

    bool bPaused = true;

    QJniObject mediaSessionHandler;

    int currentVolume = 50;
    qint64 currentDuration = 0;
};

#endif
