//
// Created by Stalker7274 on 24.03.2025.
//

#ifndef PLAYERSUBSYSTEM_H
#define PLAYERSUBSYSTEM_H

#include <QAudioOutput>
#include <QObject>
#include <QDir>
#include <QMediaPlayer>
#include "songPath.h"

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

    void SetVolume(float volume) const;

    float getVolume() const { return audioOutput->volume(); };

    void NextSong();

    void PreviousSong();

    QList<SongPath> getSongNames() const{ return songs;};

    void addSong(SongPath Song);

    void addSongToQueue(SongPath Song);

    void savePlaylist() const;

public slots:

    static void PlayerError(QMediaPlayer::Error Error, const QString &error);

signals:

    void playlistUpdated();

private:

    QList<SongPath> getSongs();

    QAudioOutput *audioOutput;
    QMediaPlayer *player;

    QObject* Parent;

    const QString DefaultMusicFolder = QDir::currentPath() + "/Music";
    const QString DefaultMediaLibFolder = DefaultMusicFolder + "/MediaLib";

    QString CurrentSong;

    QStringList Songs;

    QList<SongPath> songs;

    QList<song> currentPlaylist;

    qint32 CurrentSongIndex;

    bool bUseQueue = false;

    QList<SongPath> queueSongs;
};

#endif
