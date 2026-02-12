//
// Created by Stalker7274 on 17.04.2025.
//

#include "AppInstance.h"
#include "UIPluginSystem.h"
#include "FileManager/FileManager.h"
#include "PlayerSubsystem.h"
#include "staticData.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFile>

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

AppInstance::AppInstance()
    : uiLoader(nullptr)
    , mainWindow(nullptr)
{
    qDebug() << "AppInstance created";
}

AppInstance::~AppInstance() {
    delete uiLoader;
    qDebug() << "AppInstance destroyed";
}

void AppInstance::initializeUISystem(bool usePlugins)
{
    if (uiLoader) {
        qDebug() << "UI system already initialized";
        return;
    }

    uiLoader = new UIPluginLoader(this);

    // Try to load config from file
    QString configPath = QDir::currentPath() + "/ui_config.json";
    if (QFile::exists(configPath)) {
        qDebug() << "Loading UI config from:" << configPath;
        uiLoader->loadConfigFromFile(configPath);
    } else {
        qDebug() << "UI config file not found:" << configPath << "- using default configuration";
    }

    qDebug() << "UI system initialized. Available UIs:" << uiLoader->getAvailableUIs();
}

QWidget* AppInstance::createUIFromConfig(const QString& uiName)
{
    if (!uiLoader) {
        qDebug() << "UI system not initialized. Call initializeUISystem() first.";
        return nullptr;
    }

    QWidget* window = nullptr;

    if (uiName.isEmpty()) {
        // Create default UI for platform
        window = uiLoader->createDefaultUI();
    } else {
        // Create specific UI
        window = uiLoader->createUI(uiName);
    }

    if (window) {
        mainWindow = window;
        window->show();
    }

    return window;
}
