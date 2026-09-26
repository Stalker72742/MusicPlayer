//
// Created by Stalker7274 on 07.04.2026.
//

#ifndef SOUNDLINK_PLUGINLOADER_H
#define SOUNDLINK_PLUGINLOADER_H

#include <QList>
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

#ifdef MYLIB_BUILD
#  define MYLIB_EXPORT Q_DECL_EXPORT
#else
#  define MYLIB_EXPORT Q_DECL_IMPORT
#endif

class MYLIB_EXPORT PluginLoader {

protected:

    static QList<LoadedPlugin> loadedPlugins;

public:
    // Загружает DLL и создаёт объект
    // Возвращает nullptr если что-то пошло не так
    static LoadedPlugin load(const QString& path);

    // Шатдаун + выгрузка DLL
    static void unload(LoadedPlugin& plugin);

    static void FindAndLoadPlugins();
};

#endif //SOUNDLINK_PLUGINLOADER_H
