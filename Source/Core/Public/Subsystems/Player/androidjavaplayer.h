#ifndef ANDROIDJAVAPLAYER_H
#define ANDROIDJAVAPLAYER_H

#pragma once
#include <functional>
#include <string>
#include <memory>
#ifdef Q_OS_ANDROID
#include <QJniObject>

#include <QThread>
#include <QTimer>
#include <atomic>

enum class PlayerState {
    Stopped,
    Playing,
    Paused,
    Loading,
    Error
};

enum class PlayerError {
    None,
    InvalidSource,
    NetworkError,
    CodecError,
    Unknown
};

struct PlayerPosition {
    int64_t currentMs = 0;
    int64_t totalMs = 0;
};

// Базовый интерфейс плеера - независим от Qt
class IPlayerBackend {
public:
    virtual ~IPlayerBackend() = default;

    // Основные методы управления
    virtual bool setSource(const std::string& source) = 0;
    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool stop() = 0;

    // Управление громкостью (0.0 - 1.0)
    virtual bool setVolume(float volume) = 0;
    virtual float getVolume() const = 0;

    // Позиция воспроизведения
    virtual bool seekTo(int64_t positionMs) = 0;
    virtual PlayerPosition getPosition() const = 0;

    // Состояние плеера
    virtual PlayerState getState() const = 0;
    virtual PlayerError getLastError() const = 0;
    virtual std::string getErrorString() const = 0;

    // Инициализация/очистка
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;

    // Callbacks - устанавливаются снаружи
    std::function<void(PlayerState)> onStateChanged;
    std::function<void(PlayerPosition)> onPositionChanged;
    std::function<void(PlayerError, const std::string&)> onError;
    std::function<void()> onEndOfMedia;

protected:
    // Вспомогательные методы для вызова callbacks
    void notifyStateChanged(PlayerState state) {
        if (onStateChanged) onStateChanged(state);
    }

    void notifyPositionChanged(const PlayerPosition& pos) {
        if (onPositionChanged) onPositionChanged(pos);
    }

    void notifyError(PlayerError error, const std::string& message) {
        if (onError) onError(error, message);
    }

    void notifyEndOfMedia() {
        if (onEndOfMedia) onEndOfMedia();
    }
};


class AndroidJavaPlayer : public QObject, public IPlayerBackend {
    Q_OBJECT

public:
    AndroidJavaPlayer();
    ~AndroidJavaPlayer() override;

    // IPlayerBackend implementation
    bool setSource(const std::string& source) override;
    bool play() override;
    bool pause() override;
    bool stop() override;

    bool setVolume(float volume) override;
    float getVolume() const override;

    bool seekTo(int64_t positionMs) override;
    PlayerPosition getPosition() const override;

    PlayerState getState() const override;
    PlayerError getLastError() const override;
    std::string getErrorString() const override;

    bool initialize() override;
    void cleanup() override;

private slots:
    void updatePosition();
    void handleJavaCallback(int messageType, const QString& data);

private:
#ifdef Q_OS_ANDROID
    QJniObject javaPlayer;
#endif
    QTimer* positionTimer;

    std::atomic<PlayerState> currentState{PlayerState::Stopped};
    std::atomic<PlayerError> lastError{PlayerError::None};
    std::string errorString;

    mutable std::atomic<float> volume{1.0f};
    mutable PlayerPosition position;

    bool initializeJavaPlayer();
    void startPositionUpdates();
    void stopPositionUpdates();
};
#endif

#endif // ANDROIDJAVAPLAYER_H
