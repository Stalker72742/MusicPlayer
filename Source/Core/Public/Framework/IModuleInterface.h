#pragma once

#include <QString>

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual QString name() const = 0;

};

#define PLUGIN_API extern "C" __declspec(dllexport)

using CreatePluginFn  = IPlugin*(*)();
using DestroyPluginFn = void(*)(IPlugin*);