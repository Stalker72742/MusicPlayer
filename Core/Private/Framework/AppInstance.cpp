//
// Created by Stalker7274 on 17.04.2025.
//

#include "Framework/AppInstance.h"

#ifdef Q_OS_ANDROID
#include "androidmainwindow.h"
#endif

class QObject;

AppInstance::AppInstance(QObject *parent) {

    createApp();
}

AppInstance::~AppInstance() {

}

void AppInstance::createApp() {

#ifdef Q_OS_ANDROID

    androidMainWindow* x = new androidMainWindow();

    x->show();
#endif
}
