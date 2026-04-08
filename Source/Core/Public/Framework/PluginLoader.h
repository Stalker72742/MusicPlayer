//
// Created by Stalker7274 on 07.04.2026.
//

#ifndef SOUNDLINK_PLUGINLOADER_H
#define SOUNDLINK_PLUGINLOADER_H

#include "IModuleInterface.h"
#include <string>

#include "IModuleInterface.h"

#ifdef _WIN32
    #include <windows.h>
    using LibHandle = HMODULE;
#else
#include <dlfcn.h>
using LibHandle = void*;
#endif

struct LoadedPlugin {
    IPlugin*      instance = nullptr;
    DestroyPluginFn destroy = nullptr;
    LibHandle     handle  = nullptr;
};

class PluginLoader {
public:
    // Загружает DLL и создаёт объект
    // Возвращает nullptr если что-то пошло не так
    static LoadedPlugin load(const QString& path);

    // Шатдаун + выгрузка DLL
    static void unload(LoadedPlugin& plugin);

    static void FindAndLoadPlugins();
};

#endif //SOUNDLINK_PLUGINLOADER_H
