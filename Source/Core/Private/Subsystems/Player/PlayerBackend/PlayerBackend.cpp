//
// Created by Stalker7274 on 08.09.2025.
//

#include "PlayerBackend.h"

playerBackend::playerBackend(QObject *parent)
    : QObject(parent), positionTimer(new QTimer(this)) {

    positionTimer->setInterval(100);
    connect(positionTimer, &QTimer::timeout, this,
            &playerBackend::updatePosition);
}
void playerBackend::pause()
{

}
void playerBackend::unpause()
{

}
void playerBackend::setSource(const QString& source)
{

}
void playerBackend::setVolume(float volume)
{

}
void playerBackend::setPosition(qint64 position)
{

}

void playerBackend::updatePosition() {
    position.currentMs++;
    setPosition(position.currentMs);
}

float playerBackend::getVolume() const {
  return volume;
}

playerPosition playerBackend::getPosition() const {
  return position;
}

playerState playerBackend::getState() const {
  return currentState;
}

playerError playerBackend::getLastError() const {
  return lastError;
}

QString playerBackend::getErrorString() const {
  return errorString;
}
