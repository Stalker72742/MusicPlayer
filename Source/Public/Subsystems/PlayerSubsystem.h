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

class PlayerSubsystem : public QObject {
    Q_OBJECT
public:

    explicit PlayerSubsystem(QObject* parent);

    ~PlayerSubsystem() override;

    void LoadSongs();

    void PlayCurrentSong() const;

    void Resume() const;

    void Pause() const;

    void SetVolume(float volume) const;

    float getVolume() const { return audioOutput->volume(); };

    void NextSong();

    void PreviousSong();

    QList<SongPath> getSongNames() const{ return songs;};

    void addSong(SongPath Song);

public slots:
    static void PlayerError(QMediaPlayer::Error Error, const QString &error);

signals:
    void playlistUpdated();

private:

    QAudioOutput *audioOutput;

    QMediaPlayer *player;

    QObject* Parent;

    const QString DefaultMusicFolder = QDir::currentPath() + "/Music";

    QString CurrentSong;

    QStringList Songs;

    QList<SongPath> songs;

    qint32 CurrentSongIndex;
};

#endif