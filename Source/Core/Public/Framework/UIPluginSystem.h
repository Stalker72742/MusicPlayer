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
#include "UIFactory.h"

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

    // Factory instance (from registry or dynamic loading)
    IUIFactory* factory = nullptr;
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

    // UI Module registration (uses UIFactoryRegistry)
    void registerFactoriesFromRegistry();
    void registerFactory(const QString& name, IUIFactory* factory);

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
    int scanForPlugins(const QString& pluginDir = "");  // Auto-scan for .dll/.so files

signals:
    void uiConfigLoaded(const QString& name);
    void uiCreated(const QString& name);
    void pluginLoadError(const QString& name, const QString& error);

private:
    QMap<QString, UIConfig> m_configs;
    QList<IUIFactory*> m_ownedFactories;  // Factories we need to delete

    QString m_currentPlatform;
    QString detectPlatform() const;
};

#endif // UIPLUGINSYSTEM_H
