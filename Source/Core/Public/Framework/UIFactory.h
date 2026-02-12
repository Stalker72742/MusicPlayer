//
// Created for MusicPlayer
// UI Factory Interface - Zero dependency UI creation
//

#pragma once

#ifndef UIFACTORY_H
#define UIFACTORY_H

#include <QString>

// Forward declarations - NO includes!
class QWidget;
class AppInstance;

// ============================================================================
// Platform-specific export/import macros
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #ifdef UI_LIBRARY_EXPORT
        #define UI_EXPORT __declspec(dllexport)
    #else
        #define UI_EXPORT __declspec(dllimport)
    #endif
#else
    #define UI_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// IUIFactory - Pure interface for UI creation (no Qt dependency in header)
// ============================================================================

class IUIFactory {
public:
    virtual ~IUIFactory() = default;

    // Create main window
    virtual QWidget* createMainWindow() = 0;

    // Metadata
    virtual const char* getName() const = 0;
    virtual const char* getVersion() const = 0;
    virtual const char* getPlatform() const = 0;
    virtual int getPriority() const = 0;

    // Lifecycle
    virtual void initialize(AppInstance* app) = 0;
    virtual void shutdown() = 0;
};

// ============================================================================
// C-style export functions (for static or dynamic linking)
// ============================================================================

extern "C" {
    // Factory creation function type
    typedef IUIFactory* (*CreateUIFactoryFunc)();

    // Every UI module must export this function
    // Example: UI_EXPORT IUIFactory* createUIFactory() { return new NewWindowsFactory(); }
}

// ============================================================================
// UI_REGISTER_FACTORY macro - Auto-registration helper
// ============================================================================

#define UI_REGISTER_FACTORY(FactoryClass, ModuleName) \
    extern "C" UI_EXPORT IUIFactory* createUIFactory() { \
        static FactoryClass factory; \
        return &factory; \
    } \
    extern "C" UI_EXPORT const char* getUIModuleName() { \
        return ModuleName; \
    }

// ============================================================================
// UIFactoryRegistry - Registry for statically linked UI modules
// ============================================================================

#include <QMap>
#include <functional>

class UIFactoryRegistry {
public:
    using FactoryCreator = std::function<IUIFactory*()>;

    static UIFactoryRegistry& instance() {
        static UIFactoryRegistry registry;
        return registry;
    }

    void registerFactory(const QString& name, FactoryCreator creator) {
        m_factories[name] = creator;
    }

    IUIFactory* createFactory(const QString& name) {
        if (m_factories.contains(name)) {
            return m_factories[name]();
        }
        return nullptr;
    }

    QStringList getAvailableFactories() const {
        return m_factories.keys();
    }

private:
    UIFactoryRegistry() = default;
    QMap<QString, FactoryCreator> m_factories;
};

// ============================================================================
// Auto-registration helper for static linking
// ============================================================================

struct UIFactoryAutoRegister {
    UIFactoryAutoRegister(const char* name, UIFactoryRegistry::FactoryCreator creator) {
        UIFactoryRegistry::instance().registerFactory(QString::fromUtf8(name), creator);
    }
};

#define UI_AUTO_REGISTER(FactoryClass, ModuleName) \
    static UIFactoryAutoRegister __uiAutoReg_##FactoryClass( \
        ModuleName, \
        []() -> IUIFactory* { return new FactoryClass(); } \
    );

#endif // UIFACTORY_H
