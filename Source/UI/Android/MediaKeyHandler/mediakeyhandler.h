#ifndef MEDIAKEYHANDLER_H
#define MEDIAKEYHANDLER_H

#include <QGuiApplication>
#include <QKeyEvent>
#include <QObject>
#include <QDebug>

class mediaKeyHandler : public QObject
{
    Q_OBJECT
public:
    explicit mediaKeyHandler(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            qDebug() << "Called event: " << keyEvent->key();
            return handleMediaKey(keyEvent);
        }
        return QObject::eventFilter(obj, event);
    }

    bool handleMediaKey(QKeyEvent *keyEvent)
    {
        // Получаем нативный код клавиши (Android specific)
        int nativeKey = keyEvent->nativeVirtualKey();

        qDebug() << "Key pressed - Qt key:" << keyEvent->key() << "Native key:" << nativeKey;

        // Android media key codes
        switch (nativeKey) {
        case 85:  // KEYCODE_MEDIA_PLAY_PAUSE
            qDebug() << "Play/Pause pressed (native)";
            emit playPausePressed();
            return true;
        case 87:  // KEYCODE_MEDIA_NEXT
            qDebug() << "Next pressed (native)";
            emit nextPressed();
            return true;
        case 88:  // KEYCODE_MEDIA_PREVIOUS
            qDebug() << "Previous pressed (native)";
            emit previousPressed();
            return true;
        case 86:  // KEYCODE_MEDIA_STOP
            qDebug() << "Stop pressed (native)";
            emit stopPressed();
            return true;
        }

        // Standard Qt keys (fallback)
        switch (keyEvent->key()) {
        case Qt::Key_MediaPlay:
        case Qt::Key_MediaPause:
        case Qt::Key_MediaTogglePlayPause:
            qDebug() << "Play/Pause pressed (Qt)";
            emit playPausePressed();
            return true;
        case Qt::Key_MediaNext:
            qDebug() << "Next pressed (Qt)";
            emit nextPressed();
            return true;
        case Qt::Key_MediaPrevious:
            qDebug() << "Previous pressed (Qt)";
            emit previousPressed();
            return true;
        case Qt::Key_MediaStop:
            qDebug() << "Stop pressed (Qt)";
            emit stopPressed();
            return true;
        }

        return false;
    }

signals:
    void playPausePressed();
    void nextPressed();
    void previousPressed();
    void stopPressed();
};

#endif // MEDIAKEYHANDLER_H
