//
// Created by Stalker7274 on 24.03.2025.
//

#ifndef PLAYERSUBSYSTEM_H
#define PLAYERSUBSYSTEM_H

#include "PlayerBackend.h"
#include "SubsystemBase.h"

#include <QDir>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

class QSlider;
class song;

class PlayerSubsystem : public SubsystemBase {
    Q_OBJECT
public:
    explicit PlayerSubsystem(playerBackend* Backend, QObject* parent = nullptr);

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

    void initJavaPlayer();
    void registerJavaCallbacks();

    playerBackend* playerBackend;

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

    int currentVolume = 50;
    qint64 currentDuration = 0;
};

#endif
