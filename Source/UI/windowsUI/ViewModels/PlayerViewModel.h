#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;

/// @brief Playback state for the UI, backed by PlayerSubsystem. QML singleton.
class PlayerViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasTrack READ HasTrack NOTIFY trackChanged)
    Q_PROPERTY(int trackId READ GetTrackId NOTIFY trackChanged)
    Q_PROPERTY(QString title READ GetTitle NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ GetArtist NOTIFY trackChanged)
    Q_PROPERTY(QColor artTint READ GetArtTint NOTIFY trackChanged)
    Q_PROPERTY(QString artUrl READ GetArtUrl NOTIFY trackChanged)
    Q_PROPERTY(bool liked READ IsLiked NOTIFY trackChanged)
    Q_PROPERTY(QString totalText READ GetTotalText NOTIFY durationChanged)

    Q_PROPERTY(bool playing READ IsPlaying NOTIFY playingChanged)

    /// @brief loading: an online track is waiting for its stream; errorText: the last playback error of the current track.
    Q_PROPERTY(bool loading READ IsLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ GetErrorText NOTIFY errorTextChanged)

    Q_PROPERTY(qreal progress READ GetProgress NOTIFY positionChanged)
    Q_PROPERTY(QString elapsedText READ GetElapsedText NOTIFY positionChanged)

    Q_PROPERTY(qreal volume READ GetVolume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ IsShuffle NOTIFY shuffleChanged)
    /// @brief 0 off, 1 the whole queue, 2 the current track (PlayerSubsystem::RepeatMode).
    Q_PROPERTY(int repeatMode READ GetRepeatMode NOTIFY repeatChanged)

public:
    /// @brief The instance shared by C++ and QML.
    static PlayerViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static PlayerViewModel* create(QQmlEngine*, QJSEngine*);

    bool HasTrack() const { return trackData.id != 0; }
    int GetTrackId() const { return trackData.id; }
    QString GetTitle() const { return trackData.title; }
    QString GetArtist() const { return trackData.artist; }
    QColor GetArtTint() const { return trackData.artTint; }
    QString GetArtUrl() const { return trackData.artUrl; }
    bool IsLiked() const { return trackData.bLiked; }
    /// @brief Duration of the current track as text, e.g. "3:07".
    QString GetTotalText() const;

    bool IsPlaying() const;
    bool IsLoading() const;
    QString GetErrorText() const { return errorText; }

    /// @brief Position of the current track, 0..1.
    qreal GetProgress() const;
    /// @brief Position of the current track as text, e.g. "1:42".
    QString GetElapsedText() const;

    qreal GetVolume() const;
    bool IsShuffle() const;
    int GetRepeatMode() const;

    /// @brief Plays the row and queues the whole list.
    Q_INVOKABLE void play(TrackListModel* list, int row);

    /// @brief Plays just this track (e.g. a search result); resumes it if it is the current one.
    Q_INVOKABLE void playSingle(int trackId);

    /// @brief Plays the track with the whole library queued.
    Q_INVOKABLE void playFromLibrary(int trackId);
    /// @brief Play / pause.
    Q_INVOKABLE void togglePlay();
    /// @brief See PlayerSubsystem::Next().
    Q_INVOKABLE void next();
    /// @brief See PlayerSubsystem::Previous().
    Q_INVOKABLE void previous();
    /// @brief Seeks to a fraction 0..1 of the current track.
    Q_INVOKABLE void seek(qreal fraction);
    /// @brief Sets the volume, 0..1.
    Q_INVOKABLE void setVolume(qreal value);
    /// @brief Turns shuffle on or off.
    Q_INVOKABLE void toggleShuffle();
    /// @brief Cycles the repeat mode: off -> all -> one -> off.
    Q_INVOKABLE void toggleRepeat();
    /// @brief Likes or unlikes the current track.
    Q_INVOKABLE void toggleLike();

signals:
    void trackChanged();
    void durationChanged();
    void playingChanged();
    void loadingChanged();
    void errorTextChanged();
    void positionChanged();
    void volumeChanged();
    void shuffleChanged();
    void repeatChanged();

private:
    PlayerViewModel();

    /// Tags and likes of the current track can change while it plays.
    void RefreshTrack();

    void SetErrorText(const QString& text);

    TrackData trackData;
    QString errorText;
};
