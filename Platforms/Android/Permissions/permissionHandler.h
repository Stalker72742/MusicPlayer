#ifndef PERMISSIONHANDLER_H
#define PERMISSIONHANDLER_H

#include <QObject>
#include <QDebug>
#include <QJniObject>
#include <jni.h>

class PermissionHandler : public QObject {
    Q_OBJECT

public:
    static PermissionHandler* instance();
    void requestPermissions();

    static void onPermissionResult(JNIEnv *env, jobject thiz, jint requestCode, jboolean granted);

    private slots:
        void handlePermissionResult(bool granted);

    signals:
        void permissionsGranted();
    void permissionsDenied();

private:
    PermissionHandler() = default;
    ~PermissionHandler() = default;

    bool checkPermissionStates();
    bool arePermissionsPermanentlyDenied();
    QStringList getRequiredPermissions();
    void performPermissionRequest();
    void openAppSettings();
};

// JNI функции
extern "C" {
    JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved);
}

#endif // PERMISSIONHANDLER_H