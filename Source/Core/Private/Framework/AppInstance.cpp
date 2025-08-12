//
// Created by Stalker7274 on 17.04.2025.
//

#include "AppInstance.h"

AppInstance* AppInstance::instance = nullptr;

AppInstance* AppInstance::getInstance() {

    if (!instance) {
        instance = new AppInstance();
    }
    return instance;
}

AppInstance::AppInstance(QObject *parent) {

    subsystems.append(new PlayerSubsystem(this));
}

AppInstance::~AppInstance() {

}
