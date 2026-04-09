#include <QApplication>

#include "PluginLoader.h"
#include "Source/Core/Public/Framework/AppInstance.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    AppInstance* appInstance = AppInstance::getInstance();

    PluginLoader::FindAndLoadPlugins();

    const int result = app.exec();
    //ModuleManager::Get().ShutdownAllModules();

    return result;
}