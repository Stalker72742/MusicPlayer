//
// Created by Stalker7274 on 26.07.2025.
//

#include "permissionHandler.h"

#include "PermissionHandler.h"

PermissionHandler* PermissionHandler::instance() {
    static PermissionHandler* inst = new PermissionHandler();
    return inst;
}

void PermissionHandler::requestPermissions() {
    qDebug() << "Starting permission request process";

    // Сначала проверяем состояние разрешений
    if (checkPermissionStates()) {
        qDebug() << "All permissions already granted";
        return;
    }

    // Проверяем, заблокированы ли разрешения навсегда
    if (arePermissionsPermanentlyDenied()) {
        qDebug() << "Permissions permanently denied - opening settings";
        openAppSettings();
        return;
    }

    // Запрашиваем разрешения
    performPermissionRequest();
}

void PermissionHandler::onPermissionResult(JNIEnv *env, jobject thiz, jint requestCode, jboolean granted) {
    Q_UNUSED(env)
    Q_UNUSED(thiz)

    qDebug() << "Permission result received - Code:" << requestCode << "Granted:" << granted;

    if (requestCode == 1001) {
        PermissionHandler::instance()->handlePermissionResult(granted);
    }
}

void PermissionHandler::handlePermissionResult(bool granted) {
    if (granted) {
        qDebug() << "Permissions granted successfully!";
    } else {
        qDebug() << "Permissions denied by user";
    }
}

bool PermissionHandler::checkPermissionStates() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) return false;

    QStringList permissions = getRequiredPermissions();

    for (const QString &permission : permissions) {
        int result = QJniObject::callStaticMethod<jint>(
            "androidx/core/content/ContextCompat",
            "checkSelfPermission",
            "(Landroid/content/Context;Ljava/lang/String;)I",
            activity.object<jobject>(),
            QJniObject::fromString(permission).object<jstring>());

        if (result != 0) { // PERMISSION_DENIED
            return false;
        }
    }

    return true;
}

bool PermissionHandler::arePermissionsPermanentlyDenied() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) return false;

    QStringList permissions = getRequiredPermissions();

    for (const QString &permission : permissions) {
        // Если разрешение запрещено, но shouldShowRationale = false,
        // это означает постоянный запрет
        int permissionResult = QJniObject::callStaticMethod<jint>(
            "androidx/core/content/ContextCompat",
            "checkSelfPermission",
            "(Landroid/content/Context;Ljava/lang/String;)I",
            activity.object<jobject>(),
            QJniObject::fromString(permission).object<jstring>());

        if (permissionResult != 0) { // PERMISSION_DENIED
            bool shouldShowRationale = QJniObject::callStaticMethod<jboolean>(
                "androidx/core/app/ActivityCompat",
                "shouldShowRequestPermissionRationale",
                "(Landroid/app/Activity;Ljava/lang/String;)Z",
                activity.object<jobject>(),
                QJniObject::fromString(permission).object<jstring>());

            if (!shouldShowRationale) {
                qDebug() << "Permission permanently denied:" << permission;
                return true;
            }
        }
    }

    return false;
}

QStringList PermissionHandler::getRequiredPermissions() {
    int sdkVersion = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");

    if (sdkVersion >= 34) {
        return {
            "android.permission.READ_MEDIA_IMAGES",
            "android.permission.READ_MEDIA_VIDEO",
            "android.permission.READ_MEDIA_AUDIO",
            "android.permission.READ_MEDIA_VISUAL_USER_SELECTED"
        };
    } else if (sdkVersion >= 33) {
        return {
            "android.permission.READ_MEDIA_IMAGES",
            "android.permission.READ_MEDIA_VIDEO",
            "android.permission.READ_MEDIA_AUDIO"
        };
    } else {
        return {"android.permission.READ_EXTERNAL_STORAGE"};
    }
}

void PermissionHandler::performPermissionRequest() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) {
        qDebug() << "No activity available";
        return;
    }

    QStringList permissions = getRequiredPermissions();

    QJniEnvironment env;
    jobjectArray javaPermissions = env->NewObjectArray(
        permissions.size(),
        env->FindClass("java/lang/String"),
        nullptr);

    for (int i = 0; i < permissions.size(); ++i) {
        QJniObject permission = QJniObject::fromString(permissions[i]);
        env->SetObjectArrayElement(javaPermissions, i, permission.object<jstring>());
    }

    qDebug() << "Requesting permissions with proper handler";

    QJniObject::callStaticMethod<void>(
        "androidx/core/app/ActivityCompat",
        "requestPermissions",
        "(Landroid/app/Activity;[Ljava/lang/String;I)V",
        activity.object<jobject>(),
        javaPermissions,
        1001);
}

void PermissionHandler::openAppSettings() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) return;

    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                     QJniObject::fromString("android.settings.APPLICATION_DETAILS_SETTINGS").object<jstring>());

    QString packageName = activity.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();

    QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("package:" + packageName).object<jstring>());

    intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;", uri.object<jobject>());

    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());

    qDebug() << "Opening app settings";
}

// Регистрация JNI метода
static JNINativeMethod methods[] = {
    {"onPermissionResult", "(IZ)V", (void*)PermissionHandler::onPermissionResult}
};

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    Q_UNUSED(reserved)

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass clazz = env->FindClass("your/package/name/MainActivity"); // Замените на ваш пакет
    if (clazz == nullptr) {
        qDebug() << "Failed to find MainActivity class";
        return JNI_ERR;
    }

    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0])) < 0) {
        qDebug() << "Failed to register native methods";
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}