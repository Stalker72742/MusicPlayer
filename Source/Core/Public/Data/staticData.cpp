//
// Created by Stalker7274 on 05.11.2025.
//

#include "staticData.h"

#include <QDir>

#ifdef Q_OS_ANDROID
#include <QtCore/private/qandroidextras_p.h>
#endif

staticData::staticData(QObject* Parent) : SubsystemBase(Parent)
{

#ifdef Q_OS_WIN
    mediaLibFolder = QDir::currentPath() + "/Medialib";
#endif
#ifdef Q_OS_ANDROID

    QJniObject environment = QJniObject::callStaticObjectMethod(
        "android/os/Environment",
        "getExternalStoragePublicDirectory",
        "(Ljava/lang/String;)Ljava/io/File;",
        QJniObject::getStaticObjectField(
            "android/os/Environment",
            "DIRECTORY_MUSIC",
            "Ljava/lang/String;"
            ).object()
        );

    QString DefaultMusicFolder = environment.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString();
    mediaLibFolder = DefaultMusicFolder + "/Medialib";
#endif
}

QString staticData::allMediaConfigName = "AllSongs.json";

QString staticData::mediaLibFolder = "";