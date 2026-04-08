#include <iostream>
#include <QApplication>

#include "PluginLoader.h"
#include "Source/Core/Public/Framework/AppInstance.h"
#include "Source/Core/Public/Framework/IModuleInterface.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    const std::string path = "NewWindowsUI.dll";

    auto plugin = PluginLoader::load(path);

    if (!plugin.instance) {
        std::cerr << "Failed to load plugin\n";
        return 1;
    }

    plugin.instance->init();

    std::cout << "Plugin name: " << plugin.instance->name().toStdString() << "\n";

    AppInstance* appInstance = AppInstance::getInstance();

    const int result = app.exec();
    ModuleManager::Get().ShutdownAllModules();

    PluginLoader::unload(plugin);

    return result;
}