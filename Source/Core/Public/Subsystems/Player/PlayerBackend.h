//
// Created by Stalker7274 on 08.09.2025.
//

#ifndef MUSICPLAYER_IPLAYERBACKEND_H
#define MUSICPLAYER_IPLAYERBACKEND_H
#include <QTimer>

struct playerPosition {
  int64_t currentMs = 0;
  int64_t totalMs = 0;
};

enum class EPlayerState {
    Loading,
    Playing,
    Stopped,
    Finished,
    Error
};

enum class EPlayerError {
  None,
  InvalidSource,
  NetworkError,
  CodecError,
  Unknown
};

class QString;

class playerBackend : public QObject {
  Q_OBJECT

public:
    explicit playerBackend(QObject* parent);

    virtual void play() = 0;
    virtual void pause();
    virtual void unpause();
    virtual void setSource(const QString& source);
    virtual void setVolume(float volume);
    virtual void setPosition(qint64 position);
    virtual void updatePosition();

signals:
    void onUpdatePosition(playerPosition position);
    void onPlayerStateChanged(EPlayerState state);

public:
    virtual float getVolume() const;
    virtual playerPosition getPosition() const;
    virtual EPlayerState getState() const;
    virtual EPlayerError getLastError() const;
    virtual QString getErrorString() const;

    QTimer* positionTimer;

    EPlayerState currentState{EPlayerState::Stopped};
    EPlayerError lastError{EPlayerError::None};
    QString errorString;

    float volume {100.0f};
    playerPosition position;
};

#endif // MUSICPLAYER_IPLAYERBACKEND_H
