//
// Created by Stalker7274 on 24.03.2025.
//

#ifndef PLAYERSUBSYSTEM_H
#define PLAYERSUBSYSTEM_H
#include <QAudioOutput>
#include <QObject>
#include <QDir>
#include <QMediaPlayer>

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

    QStringList getSongNames() const{ return Songs;};

public slots:
    static void PlayerError(QMediaPlayer::Error Error, const QString &error);

private:

    QAudioOutput *audioOutput;

    QMediaPlayer *player;

    QObject* Parent;

    const QString DefaultMusicFolder = QDir::currentPath() + "/Music";

    QString CurrentSong;

    QStringList Songs;

    qint32 CurrentSongIndex;
};

#endif