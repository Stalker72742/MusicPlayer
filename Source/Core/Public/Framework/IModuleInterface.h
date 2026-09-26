#pragma once

#include <QString>

#ifdef MYLIB_BUILD
    #define MYLIB_EXPORT Q_DECL_EXPORT
#else
    #define MYLIB_EXPORT Q_DECL_IMPORT
#endif

class MYLIB_EXPORT IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual QString name() const = 0;
};

#define PLUGIN_API extern "C" __declspec(dllexport)

using CreatePluginFn  = IPlugin*(*)();
using DestroyPluginFn = void(*)(IPlugin*);