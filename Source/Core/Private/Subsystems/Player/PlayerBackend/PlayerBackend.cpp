//
// Created by Stalker7274 on 08.09.2025.
//

#include "PlayerBackend.h"

playerBackend::playerBackend(QObject *parent) : QObject(parent) {

}
void playerBackend::pause() {}
void playerBackend::unpause() {}
void playerBackend::setSource(QString Path) {}
void playerBackend::setVolume(float volume) {}