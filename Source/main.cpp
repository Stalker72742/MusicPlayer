#include <QApplication>

#include "PluginLoader.h"
#include "AppConfigs.h"
#include "AppInstance.h"
#include "eventDispatcher/EventDispatcher.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    AppConfigs::RegisterAppConfigs();

    AppInstance* appInstance = AppInstance::getInstance();
    appInstance->addSubsystem(new eventDisp());

    PluginLoader::FindAndLoadPlugins();

    return QApplication::exec();
}
