//
// Created by Stalker7274 on 17.04.2025.
//

#include "AppInstance.h"

#include "mainwindow.h"


AppInstance::AppInstance(QObject *parent) {

    createApp();
}

AppInstance::~AppInstance() {

    delete MainWindow;
}

void AppInstance::createApp() {

    MainWindow = new mainWindow(this);

    MainWindow->show();
}