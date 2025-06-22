//
// Created by Stalker7274 on 19.04.2025.
//

#include "AppInstanceLibs.h"
#include "AppInstance.h"

AppInstance* AppInstanceLibs::getAppInstance(QObject *Object) {
#ifdef Q_OS_WIN
    if (!Object) {
        return nullptr;
    }

    AppInstance *App;

    if ((App = qobject_cast<AppInstance*>(Object))) {
        return App;
    }

    if (!Object->parent()) {
        return nullptr;
    }

    if ((App = qobject_cast<AppInstance*>(Object->parent()))) {
        return App;
    }

    return getAppInstance(Object->parent());
#else
    return nullptr;
#endif
}
