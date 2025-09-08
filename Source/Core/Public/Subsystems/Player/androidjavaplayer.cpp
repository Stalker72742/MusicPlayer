#include "androidjavaplayer.h"

#ifdef Q_OS_ANDROID

AndroidJavaPlayer::AndroidJavaPlayer()
    : positionTimer(new QTimer(this)) {

    positionTimer->setInterval(100); // 100ms updates
    connect(positionTimer, &QTimer::timeout, this, &AndroidJavaPlayer::updatePosition);
}

AndroidJavaPlayer::~AndroidJavaPlayer() {
    cleanup();
}

bool AndroidJavaPlayer::initialize() {
    if (!initializeJavaPlayer()) {
        lastError = PlayerError::Unknown;
        errorString = "Failed to initialize Java player";
        return false;
    }

    currentState = PlayerState::Stopped;
    lastError = PlayerError::None;
    return true;
}

void AndroidJavaPlayer::cleanup() {
    stop();
    if (javaPlayer.isValid()) {
        javaPlayer.callMethod<void>("cleanup");
    }
}

bool AndroidJavaPlayer::setSource(const std::string& source) {
    if (!javaPlayer.isValid()) return false;

    QString qSource = QString::fromStdString(source);
    bool result = javaPlayer.callMethod<jboolean>("setSource",
                                                  "(Ljava/lang/String;)Z",
                                                  QJniObject::fromString(qSource).object());

    if (result) {
        currentState = PlayerState::Loading;
        notifyStateChanged(currentState);
    }

    return result;
}

bool AndroidJavaPlayer::play() {
    if (!javaPlayer.isValid()) return false;

    bool result = javaPlayer.callMethod<jboolean>("play");
    if (result) {
        currentState = PlayerState::Playing;
        notifyStateChanged(currentState);
        startPositionUpdates();
    }

    return result;
}

bool AndroidJavaPlayer::pause() {
    if (!javaPlayer.isValid()) return false;

    bool result = javaPlayer.callMethod<jboolean>("pause");
    if (result) {
        currentState = PlayerState::Paused;
        notifyStateChanged(currentState);
        stopPositionUpdates();
    }

    return result;
}

bool AndroidJavaPlayer::stop() {
    if (!javaPlayer.isValid()) return false;

    bool result = javaPlayer.callMethod<jboolean>("stop");
    if (result) {
        currentState = PlayerState::Stopped;
        notifyStateChanged(currentState);
        stopPositionUpdates();

        position.currentMs = 0;
        notifyPositionChanged(position);
    }

    return result;
}

bool AndroidJavaPlayer::setVolume(float vol) {
    if (!javaPlayer.isValid()) return false;

    vol = std::clamp(vol, 0.0f, 1.0f);
    bool result = javaPlayer.callMethod<jboolean>("setVolume", "(F)Z", vol);
    if (result) {
        volume = vol;
    }

    return result;
}

float AndroidJavaPlayer::getVolume() const {
    return volume;
}

bool AndroidJavaPlayer::seekTo(int64_t positionMs) {
    if (!javaPlayer.isValid()) return false;

    bool result = javaPlayer.callMethod<jboolean>("seekTo", "(J)Z", static_cast<jlong>(positionMs));
    if (result) {
        position.currentMs = positionMs;
        notifyPositionChanged(position);
    }

    return result;
}

PlayerPosition AndroidJavaPlayer::getPosition() const {
    if (javaPlayer.isValid() && currentState == PlayerState::Playing) {
        // Получаем актуальную позицию из Java
        jlong current = javaPlayer.callMethod<jlong>("getCurrentPosition");
        jlong duration = javaPlayer.callMethod<jlong>("getDuration");

        position.currentMs = static_cast<int64_t>(current);
        position.totalMs = static_cast<int64_t>(duration);
    }

    return position;
}

PlayerState AndroidJavaPlayer::getState() const {
    return currentState;
}

PlayerError AndroidJavaPlayer::getLastError() const {
    return lastError;
}

std::string AndroidJavaPlayer::getErrorString() const {
    return errorString;
}

bool AndroidJavaPlayer::initializeJavaPlayer() {
    try {
        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative",
            "activity",
            "()Landroid/app/Activity;"
            );

        if (!activity.isValid()) {
            return false;
        }

        javaPlayer = QJniObject("com/example/MusicPlayer/NativeAudioPlayer",
                                "(Landroid/content/Context;J)V",
                                activity.object(),
                                reinterpret_cast<jlong>(this));

        return javaPlayer.isValid();

    } catch (...) {
        return false;
    }
}

void AndroidJavaPlayer::startPositionUpdates() {
    if (currentState == PlayerState::Playing) {
        positionTimer->start();
    }
}

void AndroidJavaPlayer::stopPositionUpdates() {
    positionTimer->stop();
}

void AndroidJavaPlayer::updatePosition() {
    if (currentState == PlayerState::Playing) {
        PlayerPosition pos = getPosition();
        notifyPositionChanged(pos);

        // Проверяем окончание медиа
        if (pos.totalMs > 0 && pos.currentMs >= pos.totalMs) {
            currentState = PlayerState::Stopped;
            notifyStateChanged(currentState);
            notifyEndOfMedia();
            stopPositionUpdates();
        }
    }
}

void AndroidJavaPlayer::handleJavaCallback(int messageType, const QString& data) {
    switch (messageType) {
    case 0: // STATE_CHANGED
        // Обработка изменения состояния из Java
        break;
    case 1: // ERROR
        lastError = PlayerError::Unknown;
        errorString = data.toStdString();
        notifyError(lastError, errorString);
        break;
    case 2: // END_OF_MEDIA
        notifyEndOfMedia();
        break;
    }
}

// JNI callback из Java
extern "C" JNIEXPORT void JNICALL
Java_com_example_MusicPlayer_NativeAudioPlayer_nativeCallback(
    JNIEnv* env, jobject thiz, jlong nativePtr, jint messageType, jstring data) {

    AndroidJavaPlayer* player = reinterpret_cast<AndroidJavaPlayer*>(nativePtr);
    if (player) {
        QString dataStr = QJniObject(data).toString();

        // Вызываем через Qt's meta system для thread safety
        QMetaObject::invokeMethod(player, "handleJavaCallback",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, messageType),
                                  Q_ARG(QString, dataStr));
    }
}
#endif