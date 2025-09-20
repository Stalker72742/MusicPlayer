#ifndef ANDROIDJAVAPLAYER_H
#define ANDROIDJAVAPLAYER_H

#pragma once
#include <functional>

#include "../../../Core/Public/Subsystems/Player/PlayerBackend.h"

#include <QJniObject>

#include <QThread>
#include <QTimer>
#include <atomic>

class AndroidJavaPlayer : public playerBackend {
    Q_OBJECT

public:
    AndroidJavaPlayer(QObject* parent);
    ~AndroidJavaPlayer() override;

    void setSource(const QString& source) override;
    void play() override;
    void pause() override;

    void setVolume(float volume) override;
    void setPosition(qint64 position) override;

private slots:
    void updatePosition() override;
    void handleJavaCallback(int messageType, const QString& data);

private:
    void startPositionUpdates();
    void stopPositionUpdates();

    QJniObject playerService;
    QJniObject mediaSessionHandler;
};

#endif // ANDROIDJAVAPLAYER_H
