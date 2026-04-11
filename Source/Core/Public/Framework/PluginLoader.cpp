//
// Created by Stalker7274 on 07.04.2026.
//

#include "PluginLoader.h"

#include <iostream>
#include <QDirIterator>


LoadedPlugin PluginLoader::load(const QString& path) {

    QString name = QFileInfo(path).fileName().toLower();
    const bool isQtDll =  name.startsWith("qt") ||
           name.startsWith("qml") ||
           name.startsWith("qtquick");

    if (isQtDll) {
        return LoadedPlugin();
    }

    LoadedPlugin result;

    std::string stdPath = path.toStdString();

#ifdef _WIN32
    result.handle = LoadLibraryA(stdPath.c_str());
    if (!result.handle) {
        std::cerr << "[PluginLoader] LoadLibrary failed: " << stdPath
                  << " (error " << GetLastError() << ")\n";
        return result;
    }

    auto create  = (CreatePluginFn) GetProcAddress(result.handle, "createPlugin");
    auto destroy = (DestroyPluginFn)GetProcAddress(result.handle, "destroyPlugin");
#else
    result.handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!result.handle) {
        std::cerr << "[PluginLoader] dlopen failed: " << dlerror() << "\n";
        return result;
    }

    auto create  = (CreatePluginFn) dlsym(result.handle, "createPlugin");
    auto destroy = (DestroyPluginFn)dlsym(result.handle, "destroyPlugin");
#endif

    if (!create || !destroy) {
        std::cerr << "[PluginLoader] Missing createPlugin/destroyPlugin in: " << stdPath << "\n";
        unload(result);
        return result;
    }

    result.instance = create();
    result.destroy  = destroy;

    std::cout << "[PluginLoader] Loaded: " << result.instance->name().toStdString() << "\n";
    return result;
}

void PluginLoader::unload(LoadedPlugin &plugin) {

    if (plugin.instance && plugin.destroy) {
        plugin.instance->shutdown();
        plugin.destroy(plugin.instance);
        plugin.instance = nullptr;
    }

#ifdef _WIN32
    if (plugin.handle) FreeLibrary(plugin.handle);
#else
    if (plugin.handle) dlclose(plugin.handle);
#endif

    plugin.handle = nullptr;
}

void PluginLoader::FindAndLoadPlugins() {

    QDirIterator it( QDir::currentPath() + "/Plugins", {"*.dll"},
        QDir::Files, QDirIterator::Subdirectories );

    while (it.hasNext()) {

        const QString& pluginPath = it.next();

        const auto plugin = load(it.fileInfo().filePath().remove(QDir::currentPath() + "/"));

        qDebug() << "Trying to load: " << it.fileInfo().filePath().remove(QDir::currentPath());

        if (plugin.instance) {

            qDebug() << "Loaded plugin: " << plugin.instance->name();
            plugin.instance->init();
        }
    }
}