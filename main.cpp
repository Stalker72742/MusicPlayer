#include <iostream>
#include <QApplication>
#include <QDirIterator>

#include "PluginLoader.h"
#include "Source/Core/Public/Framework/AppInstance.h"
#include "Source/Core/Public/Framework/IModuleInterface.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    PluginLoader::FindAndLoadPlugins();

    AppInstance* appInstance = AppInstance::getInstance();

    const int result = app.exec();
    ModuleManager::Get().ShutdownAllModules();

    return result;
}