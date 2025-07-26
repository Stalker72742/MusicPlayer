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

#include "songPath.h"

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

    void SetVolume(int volume) const;

    int getVolume() const { return static_cast<int>(audioOutput->volume() * 100.0f); };

    void NextSong();

    void PreviousSong();

    void addSong(song* Song);

    void addSongToQueue(song* Song);

    void savePlaylist();

    QList<song*> getSongs();

    QString createPlaylist(QString playlistName);

    void setCurrentPlaylist(QString playlistPathLocal);

    void removePlaylist(QString playlistPath);

    QList<QString> getPlaylists();

    /// playlist maybe name = QString or path = QDir
    template <typename P>
    void addSongToPlaylist(song* Song, P playlist);

    void addSongToPlaylistByName(QString Song, QString playlistName);

    qint64 getMaxDuration() const { return player->duration(); }

    QList<QString> getLocalSongsPaths() const;

    void showMediaLib();

    QString getMediaLibFolder() { return DefaultMediaLibFolder; }

public slots:

    void PlayerError(QMediaPlayer::Error Error, const QString &error);

    void onAudioStreamLoaded(song* songPtr);

    void updateSliderPosition();

signals:

    void playlistUpdated();

    void playlistChanged();

    void updateMusicDuration(int newDuration);

    void onShowMediaLib(QList<QString> songs);

private:

    void checkMusicFolder();

    QAudioOutput *audioOutput;
    QMediaPlayer *player;

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
};

#endif
