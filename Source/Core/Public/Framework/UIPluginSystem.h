//
// Created for MusicPlayer
// UI Plugin System - Dynamic UI loading architecture
//

#pragma once

#ifndef UIPLUGINSYSTEM_H
#define UIPLUGINSYSTEM_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include <functional>

// Forward declarations
class QMainWindow;
class QWidget;
class AppInstance;

// ============================================================================
// UIConfig - Configuration for UI modules
// ============================================================================

struct UIConfig {
    QString name;                           // UI module name (e.g., "NewWindows", "OldWindows")
    QString platform;                       // Target platform ("windows", "android", "all")
    QString libraryPath;                    // Path to shared library (for plugin mode)
    QString mainWindowClass;                // Main window class name
    QJsonObject settings;                   // Additional settings
    int priority = 0;                       // Priority for selection (higher = preferred)
    bool enabled = true;                    // Is this UI enabled?

    // Factory function type for creating windows
    using WindowFactory = std::function<QWidget*()>;
    WindowFactory factory = nullptr;        // Factory function for static linking
};

// ============================================================================
// IUIModule - Interface for UI modules
// ============================================================================

class IUIModule {
public:
    virtual ~IUIModule() = default;

    // Create main window
    virtual QWidget* createMainWindow() = 0;

    // Get module info
    virtual QString getName() const = 0;
    virtual QString getVersion() const = 0;
    virtual QString getPlatform() const = 0;

    // Lifecycle hooks
    virtual void initialize(AppInstance* app) = 0;
    virtual void shutdown() = 0;
};

// ============================================================================
// UIPluginLoader - Loads and manages UI plugins/modules
// ============================================================================

class UIPluginLoader : public QObject {
    Q_OBJECT
public:
    explicit UIPluginLoader(QObject* parent = nullptr);
    ~UIPluginLoader() override;

    // Configuration loading
    bool loadConfigFromFile(const QString& configPath);
    bool loadConfigFromJson(const QJsonObject& json);
    void addUIConfig(const UIConfig& config);

    // UI Module registration (for static linking)
    void registerUIModule(const QString& name, UIConfig::WindowFactory factory);

    // UI Selection
    UIConfig* selectBestUI(const QString& platform);
    UIConfig* getUIConfig(const QString& name);
    QStringList getAvailableUIs() const;

    // UI Creation
    QWidget* createUI(const QString& name);
    QWidget* createDefaultUI();

    // Plugin loading (for dynamic libraries)
    bool loadPlugin(const QString& libraryPath);
    void unloadPlugin(const QString& name);

signals:
    void uiConfigLoaded(const QString& name);
    void uiCreated(const QString& name);
    void pluginLoadError(const QString& name, const QString& error);

private:
    QMap<QString, UIConfig> m_configs;
    QMap<QString, IUIModule*> m_plugins;

    QString m_currentPlatform;
    QString detectPlatform() const;
};

// ============================================================================
// Macros for easy UI module registration
// ============================================================================

#define REGISTER_UI_MODULE(loader, name, factoryFunc) \
    loader->registerUIModule(name, []() -> QWidget* { return new factoryFunc(); })

#endif // UIPLUGINSYSTEM_H
