//
// Created by Stalker7274 on 17.04.2025.
//

#include "AppInstance.h"
#include "FileManager/FileManager.h"
#include "PlayerSubsystem.h"
#include "staticData.h"

AppInstance * AppInstance::instance = nullptr;

AppInstance* AppInstance::getInstance() {

    if (!instance) {
        instance = new AppInstance();
    }
    return instance;
}

void AppInstance::addSubsystem(SubsystemBase* subsystem)
{
    subsystems.push_back(subsystem);
}

AppInstance::AppInstance() {


}

AppInstance::~AppInstance() {

}
