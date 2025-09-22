

#include "androidjavaplayer.h"
#include <private/qandroidextras_p.h>

AndroidJavaPlayer::AndroidJavaPlayer(QObject* parent)
    : playerBackend(parent) {

    QTimer::singleShot(500, [this](){
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            qWarning() << "No Android context!";
            return;
        }
        // Создаём Intent для сервиса
        playerService = QJniObject("android/content/Intent", "()V");
        playerService.callObjectMethod(
            "setClassName",
            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
            QJniObject::fromString("com.example.MusicPlayer").object<jstring>(),
            QJniObject::fromString("com.example.MusicPlayer.MusicPlayerService").object<jstring>());

        if (!playerService.isValid()) {
            qWarning() << "Failed to create intent!";
            return;
        }
        QJniObject Object = activity.callObjectMethod(
            "startForegroundService",
            "(Landroid/content/Intent;)Landroid/content/ComponentName;",
            playerService.object());

        if (!Object.isValid()) {
            qWarning() << "Failed to start MusicPlayerService";
            return;
        }

        qDebug() << "MusicPlayerService started successfully";

        QTimer::singleShot(300, [this](){
            setVolume(100);
            setSource("/storage/emulated/0/Music/Judas.mp3");
        });
        //play();
    });
}

AndroidJavaPlayer::~AndroidJavaPlayer() {

}

void AndroidJavaPlayer::setSource(const QString& source) {

    if (!playerService.isValid()) return;

    bool result = QJniObject::callStaticMethod<jboolean>(
        "com/example/MusicPlayer/MusicPlayerService",
        "playSongStatic",
        "(Ljava/lang/String;)Z",
        QJniObject::fromString(source).object<jstring>());

    if (result) {
        currentState = playerState::Loading;
    }else{
        qWarning() << "Fail to set source and play";
    }
}

void AndroidJavaPlayer::play() {
    if (!playerService.isValid()) return;

    bool result = playerService.callMethod<jboolean>("play");
    if (result) {
        currentState = playerState::Playing;
        startPositionUpdates();
    }
}

void AndroidJavaPlayer::pause() {
    if (!playerService.isValid()) return;

    bool result = playerService.callMethod<jboolean>("pause");
    if (result) {
        currentState = playerState::Paused;
        stopPositionUpdates();
    }

}

void AndroidJavaPlayer::setVolume(float vol) {
    if (!playerService.isValid()) return;

    vol = std::clamp(vol, 0.0f, 100.0f);
    bool result = QJniObject::callStaticMethod<jboolean>(
        "com/example/MusicPlayer/MusicPlayerService",
        "setVolumeStatic",
        "(F)Z",
        vol);
    if (result) {
        volume = vol;
    }
}

void AndroidJavaPlayer::setPosition(qint64 positionMs) {
    if (!playerService.isValid()) return;

    bool result = playerService.callMethod<jboolean>("seekTo", "(J)Z", static_cast<jlong>(positionMs));
    if (result) {
        position.currentMs = positionMs;
    }
}

void AndroidJavaPlayer::startPositionUpdates() {
    if (currentState == playerState::Playing) {
        positionTimer->start();
    }
}

void AndroidJavaPlayer::stopPositionUpdates() {
    positionTimer->stop();
}

void AndroidJavaPlayer::updatePosition() {
    if (currentState == playerState::Playing) {
        playerPosition pos = getPosition();

        if (pos.totalMs > 0 && pos.currentMs >= pos.totalMs) {
            currentState = playerState::Stopped;
            stopPositionUpdates();
        }
    }
}

void AndroidJavaPlayer::handleJavaCallback(int messageType, const QString& data) {
    switch (messageType) {
    case 0:
        break;
    case 1:
        lastError = playerError::Unknown;
        errorString = data;
        break;
    case 2:
        break;
    }
}

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

extern "C" JNIEXPORT void JNICALL
Java_com_example_MusicPlayer_MusicPlayerService_onPlaybackStateChanged(
    JNIEnv *env,
    jobject thiz,
    jint state)
{
    qDebug() << "🔥 Playback state changed:" << state;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MusicPlayer_MusicPlayerService_onError(JNIEnv *env, jobject thiz, jstring error){

    qDebug() << error;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MusicPlayer_MusicPlayerService_onMediaButton(JNIEnv *env, jobject thiz, jint keyCode, jstring action){

    qDebug() << "On hot key:" << keyCode;
}
