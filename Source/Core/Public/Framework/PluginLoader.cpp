//
// Created by Stalker7274 on 07.04.2026.
//

#include "PluginLoader.h"

#include <iostream>


LoadedPlugin PluginLoader::load(const std::string &path) {

    LoadedPlugin result;

#ifdef _WIN32
    result.handle = LoadLibraryA(path.c_str());
    if (!result.handle) {
        std::cerr << "[PluginLoader] LoadLibrary failed: " << path
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
        std::cerr << "[PluginLoader] Missing createPlugin/destroyPlugin in: " << path << "\n";
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