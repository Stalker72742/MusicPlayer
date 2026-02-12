//
// Created for MusicPlayer
// UI Plugin System implementation
//

#include "UIPluginSystem.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QLibrary>
#include <QDir>

// ============================================================================
// UIPluginLoader Implementation
// ============================================================================

UIPluginLoader::UIPluginLoader(QObject* parent)
    : QObject(parent)
{
    m_currentPlatform = detectPlatform();
    qDebug() << "UIPluginLoader initialized for platform:" << m_currentPlatform;
}

UIPluginLoader::~UIPluginLoader()
{
    // Cleanup plugins
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value()) {
            it.value()->shutdown();
            delete it.value();
        }
    }
}

QString UIPluginLoader::detectPlatform() const
{
#ifdef Q_OS_WIN
    return "windows";
#elif defined(Q_OS_ANDROID)
    return "android";
#elif defined(Q_OS_LINUX)
    return "linux";
#elif defined(Q_OS_MAC)
    return "macos";
#else
    return "unknown";
#endif
}

bool UIPluginLoader::loadConfigFromFile(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open UI config file:" << configPath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "Invalid UI config JSON in file:" << configPath;
        return false;
    }

    return loadConfigFromJson(doc.object());
}

bool UIPluginLoader::loadConfigFromJson(const QJsonObject& json)
{
    if (!json.contains("ui_modules")) {
        qDebug() << "No 'ui_modules' found in config";
        return false;
    }

    QJsonArray modules = json["ui_modules"].toArray();

    for (const QJsonValue& value : modules) {
        if (!value.isObject()) continue;

        QJsonObject moduleObj = value.toObject();

        UIConfig config;
        config.name = moduleObj["name"].toString();
        config.platform = moduleObj["platform"].toString("all");
        config.libraryPath = moduleObj["library_path"].toString();
        config.mainWindowClass = moduleObj["main_window_class"].toString();
        config.priority = moduleObj["priority"].toInt(0);
        config.enabled = moduleObj["enabled"].toBool(true);
        config.settings = moduleObj["settings"].toObject();

        if (config.name.isEmpty()) {
            qDebug() << "Skipping UI module with empty name";
            continue;
        }

        addUIConfig(config);
    }

    return true;
}

void UIPluginLoader::addUIConfig(const UIConfig& config)
{
    m_configs[config.name] = config;
    qDebug() << "Registered UI config:" << config.name << "for platform:" << config.platform;
    emit uiConfigLoaded(config.name);
}

void UIPluginLoader::registerUIModule(const QString& name, UIConfig::WindowFactory factory)
{
    if (m_configs.contains(name)) {
        m_configs[name].factory = factory;
        qDebug() << "Registered UI factory for:" << name;
    } else {
        // Create new config for this module
        UIConfig config;
        config.name = name;
        config.platform = m_currentPlatform;
        config.factory = factory;
        config.enabled = true;
        addUIConfig(config);
    }
}

UIConfig* UIPluginLoader::selectBestUI(const QString& platform)
{
    UIConfig* bestConfig = nullptr;
    int highestPriority = -1;

    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        UIConfig& config = it.value();

        // Skip disabled modules
        if (!config.enabled) continue;

        // Check platform compatibility
        if (config.platform != "all" && config.platform != platform) continue;

        // Check if factory or plugin is available
        if (!config.factory && config.libraryPath.isEmpty()) continue;

        // Select highest priority
        if (config.priority > highestPriority) {
            highestPriority = config.priority;
            bestConfig = &config;
        }
    }

    if (bestConfig) {
        qDebug() << "Selected UI:" << bestConfig->name << "with priority:" << bestConfig->priority;
    } else {
        qDebug() << "No suitable UI found for platform:" << platform;
    }

    return bestConfig;
}

UIConfig* UIPluginLoader::getUIConfig(const QString& name)
{
    if (m_configs.contains(name)) {
        return &m_configs[name];
    }
    return nullptr;
}

QStringList UIPluginLoader::getAvailableUIs() const
{
    QStringList uiList;
    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        if (it.value().enabled) {
            uiList.append(it.key());
        }
    }
    return uiList;
}

QWidget* UIPluginLoader::createUI(const QString& name)
{
    UIConfig* config = getUIConfig(name);
    if (!config) {
        qDebug() << "UI config not found:" << name;
        return nullptr;
    }

    QWidget* window = nullptr;

    // Try factory first (static linking)
    if (config->factory) {
        qDebug() << "Creating UI using factory:" << name;
        window = config->factory();
    }
    // Try plugin loading (dynamic library)
    else if (!config->libraryPath.isEmpty()) {
        qDebug() << "Loading UI plugin from:" << config->libraryPath;
        if (loadPlugin(config->libraryPath)) {
            if (m_plugins.contains(name)) {
                window = m_plugins[name]->createMainWindow();
            }
        }
    }

    if (window) {
        qDebug() << "Successfully created UI:" << name;
        emit uiCreated(name);
    } else {
        qDebug() << "Failed to create UI:" << name;
    }

    return window;
}

QWidget* UIPluginLoader::createDefaultUI()
{
    // Try to find best UI for current platform
    UIConfig* config = selectBestUI(m_currentPlatform);
    if (config) {
        return createUI(config->name);
    }

    qDebug() << "No default UI available";
    return nullptr;
}

bool UIPluginLoader::loadPlugin(const QString& libraryPath)
{
    QLibrary library(libraryPath);

    if (!library.load()) {
        QString error = library.errorString();
        qDebug() << "Failed to load plugin library:" << libraryPath << "Error:" << error;
        emit pluginLoadError(libraryPath, error);
        return false;
    }

    // Get plugin entry point
    typedef IUIModule* (*CreateModuleFunc)();
    CreateModuleFunc createModule = (CreateModuleFunc)library.resolve("createUIModule");

    if (!createModule) {
        qDebug() << "Failed to resolve 'createUIModule' function in plugin:" << libraryPath;
        library.unload();
        return false;
    }

    // Create module instance
    IUIModule* module = createModule();
    if (!module) {
        qDebug() << "Failed to create module instance from plugin:" << libraryPath;
        library.unload();
        return false;
    }

    QString moduleName = module->getName();
    m_plugins[moduleName] = module;

    qDebug() << "Successfully loaded plugin:" << moduleName << "version:" << module->getVersion();
    return true;
}

void UIPluginLoader::unloadPlugin(const QString& name)
{
    if (m_plugins.contains(name)) {
        IUIModule* module = m_plugins[name];
        module->shutdown();
        delete module;
        m_plugins.remove(name);
        qDebug() << "Unloaded plugin:" << name;
    }
}
