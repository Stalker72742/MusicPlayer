//
// Created by Stalker7274 on 01.07.2026.
//

#pragma once

#include "IModuleInterface.h"

template<typename T>
IPlugin* createPluginT() { return new T(); }

inline void destroyPluginImpl(IPlugin* p) { delete p; }

#define PLUGIN_EXPORT(ClassName) \
    extern "C" { \
        PLUGIN_API IPlugin* createPlugin() { return createPluginT<ClassName>(); } \
        PLUGIN_API void destroyPlugin(IPlugin* p) { destroyPluginImpl(p); } \
    }
