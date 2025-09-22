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

enum class playerState {
  Stopped,
  Playing,
  Paused,
  Loading,
  Error
};

enum class playerError {
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

public:
    virtual float getVolume() const;
    virtual playerPosition getPosition() const;
    virtual playerState getState() const;
    virtual playerError getLastError() const;
    virtual QString getErrorString() const;

    QTimer* positionTimer;

    playerState currentState{playerState::Stopped};
    playerError lastError{playerError::None};
    QString errorString;

    float volume {100.0f};
    playerPosition position;
};

#endif // MUSICPLAYER_IPLAYERBACKEND_H
