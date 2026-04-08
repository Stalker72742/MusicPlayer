#pragma once

#include <QString>
#include <QMap>
#include <QSharedPointer>
#include <functional>

class IModuleInterface
{
public:
    virtual ~IModuleInterface() = default;

    virtual void StartupModule() = 0;
    virtual void ShutdownModule() = 0;
    virtual QString GetModuleName() const = 0;

    bool IsInitialized() const { return bInitialized; }

friend class ModuleManager;
protected:
    bool bInitialized = false;
};

using ModuleFactory = std::function<IModuleInterface*()>;

class ModuleManager
{
public:
    static ModuleManager& Get()
    {
        static ModuleManager instance;
        return instance;
    }

    void RegisterModule(const QString& name, ModuleFactory factory)
    {
        if (!m_factories.contains(name))
            m_factories.insert(name, factory);
    }

    bool LoadModule(const QString& name)
    {
        if (m_modules.contains(name)) return true;
        if (!m_factories.contains(name))
        {
            qWarning("[ModuleManager] No factory registered for: %s", qPrintable(name));
            return false;
        }

        IModuleInterface* mod = m_factories[name]();
        if (!mod) return false;

        mod->StartupModule();
        mod->bInitialized = true;
        m_modules.insert(name, QSharedPointer<IModuleInterface>(mod));
        return true;
    }

    void ShutdownAllModules()
    {
        auto keys = m_modules.keys();
        std::reverse(keys.begin(), keys.end());
        for (const auto& k : keys)
        {
            m_modules[k]->ShutdownModule();
            m_modules[k]->bInitialized = false;
        }
        m_modules.clear();
    }

    IModuleInterface* GetModule(const QString& name) const
    {
        return m_modules.contains(name) ? m_modules[name].get() : nullptr;
    }

    template<typename T>
    T* GetModuleChecked(const QString& name) const
    {
        return dynamic_cast<T*>(GetModule(name));
    }

    bool TryLoadModule(const QString& InModName, bool bFindRecursive = true);

private:
    ModuleManager() = default;
    QMap<QString, ModuleFactory>                    m_factories;
    QMap<QString, QSharedPointer<IModuleInterface>> m_modules;
};

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