//
// Created by Stalker7274 on 17.04.2025.
//

#include "AppInstance.h"

class QObject;

AppInstance::AppInstance(QObject *parent) {

    subsystems.append(new PlayerSubsystem(this));
}

AppInstance::~AppInstance() {

}
