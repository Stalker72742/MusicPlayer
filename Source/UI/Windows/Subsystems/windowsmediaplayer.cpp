#include "windowsmediaplayer.h"
#include <QUrl>
#include <QDebug>

WindowsMediaPlayer::WindowsMediaPlayer(QObject* parent)
    : playerBackend(parent)
{
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);

    mediaPlayer->setAudioOutput(audioOutput);

    // Connect signals
    connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &WindowsMediaPlayer::onMediaStatusChanged);
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &WindowsMediaPlayer::onPlaybackStateChanged);
    connect(mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &WindowsMediaPlayer::onErrorOccurred);
    connect(mediaPlayer, &QMediaPlayer::durationChanged,
            this, &WindowsMediaPlayer::onDurationChanged);
    connect(mediaPlayer, &QMediaPlayer::positionChanged,
            this, &WindowsMediaPlayer::onPositionChanged);

    // Set default volume
    audioOutput->setVolume(100.0f);

    qDebug() << "WindowsMediaPlayer initialized";
}

WindowsMediaPlayer::~WindowsMediaPlayer()
{
    if (mediaPlayer) {
        mediaPlayer->stop();
    }
}

void WindowsMediaPlayer::setSource(const QString& source)
{
    if (!mediaPlayer) return;

    qDebug() << "Setting source:" << source;

    currentState = EPlayerState::Loading;
    emit onPlayerStateChanged(currentState);

    QUrl url = QUrl::fromLocalFile(source);
    mediaPlayer->setSource(url);
}

void WindowsMediaPlayer::play()
{
    if (!mediaPlayer) return;

    qDebug() << "Playing";

    mediaPlayer->play();
    currentState = EPlayerState::Playing;
    emit onPlayerStateChanged(currentState);

    positionTimer->start();
}

void WindowsMediaPlayer::pause()
{
    if (!mediaPlayer) return;

    qDebug() << "Pausing";

    mediaPlayer->pause();
    currentState = EPlayerState::Stopped;
    emit onPlayerStateChanged(currentState);

    positionTimer->stop();
}

void WindowsMediaPlayer::unpause()
{
    if (!mediaPlayer) return;

    qDebug() << "Unpausing";

    mediaPlayer->play();
    currentState = EPlayerState::Playing;
    emit onPlayerStateChanged(currentState);

    positionTimer->start();
}

void WindowsMediaPlayer::playPause()
{
    if (!mediaPlayer) return;

    if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        pause();
    } else {
        unpause();
    }
}

void WindowsMediaPlayer::setVolume(float vol)
{
    if (!audioOutput) return;

    vol = std::clamp(vol, 0.0f, 100.0f);

    // QAudioOutput uses 0.0 to 1.0 range
    float normalizedVolume = vol / 100.0f;
    audioOutput->setVolume(normalizedVolume);

    volume = vol;

    qDebug() << "Volume set to:" << vol;
}

void WindowsMediaPlayer::setPosition(qint64 pos)
{
    if (!mediaPlayer) return;

    mediaPlayer->setPosition(pos);
    position.currentMs = pos;
}

void WindowsMediaPlayer::updatePosition()
{
    if (currentState == EPlayerState::Playing && mediaPlayer) {
        position.currentMs = mediaPlayer->position();
        position.totalMs = mediaPlayer->duration();

        emit onUpdatePosition(position);

        // Check if song finished
        if (position.totalMs > 0 && position.currentMs >= position.totalMs) {
            currentState = EPlayerState::Finished;
            emit onPlayerStateChanged(currentState);
            positionTimer->stop();
        }
    }
}

void WindowsMediaPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "Media status changed:" << status;

    switch (status) {
        case QMediaPlayer::LoadedMedia:
            qDebug() << "Media loaded successfully";
            break;
        case QMediaPlayer::BufferedMedia:
            qDebug() << "Media buffered";
            break;
        case QMediaPlayer::EndOfMedia:
            qDebug() << "End of media";
            currentState = EPlayerState::Finished;
            emit onPlayerStateChanged(currentState);
            positionTimer->stop();
            break;
        case QMediaPlayer::InvalidMedia:
            qDebug() << "Invalid media";
            currentState = EPlayerState::Error;
            lastError = EPlayerError::InvalidSource;
            emit onPlayerStateChanged(currentState);
            break;
        default:
            break;
    }
}

void WindowsMediaPlayer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    qDebug() << "Playback state changed:" << state;

    switch (state) {
        case QMediaPlayer::PlayingState:
            currentState = EPlayerState::Playing;
            emit onPlayerStateChanged(currentState);
            break;
        case QMediaPlayer::PausedState:
            currentState = EPlayerState::Stopped;
            emit onPlayerStateChanged(currentState);
            break;
        case QMediaPlayer::StoppedState:
            currentState = EPlayerState::Stopped;
            emit onPlayerStateChanged(currentState);
            break;
    }
}

void WindowsMediaPlayer::onErrorOccurred(QMediaPlayer::Error error, const QString &errorStr)
{
    qDebug() << "Media player error:" << error << errorStr;

    currentState = EPlayerState::Error;
    errorString = errorStr;

    switch (error) {
        case QMediaPlayer::ResourceError:
            lastError = EPlayerError::InvalidSource;
            break;
        case QMediaPlayer::FormatError:
            lastError = EPlayerError::CodecError;
            break;
        case QMediaPlayer::NetworkError:
            lastError = EPlayerError::NetworkError;
            break;
        default:
            lastError = EPlayerError::Unknown;
            break;
    }

    emit onPlayerStateChanged(currentState);
}

void WindowsMediaPlayer::onDurationChanged(qint64 duration)
{
    position.totalMs = duration;
    qDebug() << "Duration changed:" << duration << "ms";
}

void WindowsMediaPlayer::onPositionChanged(qint64 pos)
{
    position.currentMs = pos;
}
