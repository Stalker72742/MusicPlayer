//
// Created by Stalker7274 on 19.04.2025.
//

#include "AppInstanceLibs.h"

#include "mainwindow.h"
#include "Public/Framework/AppInstance.h"

AppInstance* AppInstanceLibs::getAppInstance(QObject *Object) {

    if (!Object) {
        return nullptr;
    }

    AppInstance *App;

    if ((App = qobject_cast<AppInstance*>(Object))) {
        return App;
    }

    if (mainWindow* window = qobject_cast<mainWindow*>(Object)) {
        return window->getAppInstance();
    }

    if (!Object->parent()) {
        return nullptr;
    }

    if ((App = qobject_cast<AppInstance*>(Object->parent()))) {
        return App;
    }

    return getAppInstance(Object->parent());
}
