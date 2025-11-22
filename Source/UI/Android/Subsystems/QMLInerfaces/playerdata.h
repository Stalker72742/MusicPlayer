#ifndef PLAYERDATA_H
#define PLAYERDATA_H

#include <QObject>

class song;
class PlayerSubsystem;

class playerData : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(QString currentTrack READ currentTrack NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY currentArtistChanged)

public:
    explicit playerData(QObject* parent = nullptr);

    bool isPlaying() const { return m_isPlaying; }
    QString currentTrack() const { return m_currentTrack; }
    QString currentArtist() const { return m_currentArtist; }

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void previous();

private:
    bool m_isPlaying = false;
    QString m_currentTrack {"Unknown"};
    QString m_currentArtist;

    PlayerSubsystem* player;

    void SetHasPlaylist(bool InHasPlaylist);
    void setCurrentTrack(const QString& track);
    void setCurrentArtist(const QString& artist);

public slots:

    void OnPlayerSongChanged(song* InSong);
    void OnPlayerStateChanged(bool InIsPlaying);

signals:
    void isPlayingChanged();
    void currentTrackChanged();
    void currentArtistChanged();
};

#endif // PLAYERDATA_H
