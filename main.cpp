#include <QApplication>

#include "PluginLoader.h"
#include "AppInstance.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    AppInstance* appInstance = AppInstance::getInstance();

    PluginLoader::FindAndLoadPlugins();

    const int result = app.exec();

    return result;
}