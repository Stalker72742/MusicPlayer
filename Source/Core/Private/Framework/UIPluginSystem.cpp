//
// Created for MusicPlayer
// UI Plugin System implementation
//

#include "UIPluginSystem.h"
#include "UIFactory.h"
#include "AppInstance.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QLibrary>
#include <QDir>
#include <QCoreApplication>

// ============================================================================
// UIPluginLoader Implementation
// ============================================================================

UIPluginLoader::UIPluginLoader(QObject* parent)
    : QObject(parent)
{
    m_currentPlatform = detectPlatform();
    qDebug() << "UIPluginLoader initialized for platform:" << m_currentPlatform;

    // Auto-register factories from static registry
    registerFactoriesFromRegistry();
}

UIPluginLoader::~UIPluginLoader()
{
    // Cleanup owned factories
    for (IUIFactory* factory : m_ownedFactories) {
        if (factory) {
            factory->shutdown();
            delete factory;
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

void UIPluginLoader::registerFactoriesFromRegistry()
{
    UIFactoryRegistry& registry = UIFactoryRegistry::instance();
    QStringList factoryNames = registry.getAvailableFactories();

    qDebug() << "Found" << factoryNames.count() << "registered UI factories:" << factoryNames;

    for (const QString& name : factoryNames) {
        IUIFactory* factory = registry.createFactory(name);
        if (factory) {
            registerFactory(name, factory);
            m_ownedFactories.append(factory);  // We own this factory
        }
    }
}

void UIPluginLoader::registerFactory(const QString& name, IUIFactory* factory)
{
    UIConfig config;
    config.name = name;
    config.platform = QString::fromUtf8(factory->getPlatform());
    config.priority = factory->getPriority();
    config.enabled = true;
    config.factory = factory;

    addUIConfig(config);
    qDebug() << "Registered UI factory:" << name << "platform:" << config.platform << "priority:" << config.priority;
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

        // Try to find factory in registry
        UIConfig* existing = getUIConfig(config.name);
        if (existing && existing->factory) {
            // Update existing config but keep factory
            existing->priority = config.priority;
            existing->enabled = config.enabled;
            existing->settings = config.settings;
            qDebug() << "Updated config for existing factory:" << config.name;
        } else {
            addUIConfig(config);
        }
    }

    return true;
}

void UIPluginLoader::addUIConfig(const UIConfig& config)
{
    m_configs[config.name] = config;
    qDebug() << "Registered UI config:" << config.name << "for platform:" << config.platform;
    emit uiConfigLoaded(config.name);
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

        // Check if factory is available
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

    // Try factory first
    if (config->factory) {
        qDebug() << "Creating UI using factory:" << name;
        window = config->factory->createMainWindow();
    }
    // Try plugin loading (dynamic library)
    else if (!config->libraryPath.isEmpty()) {
        qDebug() << "Loading UI plugin from:" << config->libraryPath;
        if (loadPlugin(config->libraryPath)) {
            // After loading, factory should be set
            if (config->factory) {
                window = config->factory->createMainWindow();
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
    typedef IUIFactory* (*CreateUIFactoryFunc)();
    CreateUIFactoryFunc createFactory = (CreateUIFactoryFunc)library.resolve("createUIFactory");

    if (!createFactory) {
        qDebug() << "Failed to resolve 'createUIFactory' function in plugin:" << libraryPath;
        library.unload();
        return false;
    }

    // Create factory instance
    IUIFactory* factory = createFactory();
    if (!factory) {
        qDebug() << "Failed to create factory instance from plugin:" << libraryPath;
        library.unload();
        return false;
    }

    QString moduleName = QString::fromUtf8(factory->getName());
    registerFactory(moduleName, factory);
    m_ownedFactories.append(factory);  // We own this factory

    qDebug() << "Successfully loaded plugin:" << moduleName << "version:" << factory->getVersion();
    return true;
}

void UIPluginLoader::unloadPlugin(const QString& name)
{
    UIConfig* config = getUIConfig(name);
    if (config && config->factory) {
        config->factory->shutdown();
        m_ownedFactories.removeOne(config->factory);
        delete config->factory;
        config->factory = nullptr;
        qDebug() << "Unloaded plugin:" << name;
    }
}

int UIPluginLoader::scanForPlugins(const QString& pluginDir)
{
    QString searchDir = pluginDir;
    if (searchDir.isEmpty()) {
        // Use application directory by default
        searchDir = QCoreApplication::applicationDirPath();
    }

    qDebug() << "Scanning for UI plugins in:" << searchDir;

    QDir dir(searchDir);
    if (!dir.exists()) {
        qDebug() << "Plugin directory does not exist:" << searchDir;
        return 0;
    }

    // Look for UI .dll/.so files (TestUI.dll, NewWindowsUI.dll, OldWindowsUI.dll)
    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*UI.dll" << "TestUI.dll";
#else
    filters << "lib*UI.so" << "libTestUI.so";
#endif

    QStringList pluginFiles = dir.entryList(filters, QDir::Files);
    qDebug() << "Found" << pluginFiles.count() << "potential UI plugin files:" << pluginFiles;

    int loadedCount = 0;
    for (const QString& fileName : pluginFiles) {
        QString fullPath = dir.absoluteFilePath(fileName);
        qDebug() << "Attempting to load plugin:" << fullPath;

        if (loadPlugin(fullPath)) {
            loadedCount++;
            qDebug() << "Successfully loaded plugin:" << fileName;
        } else {
            qDebug() << "Failed to load plugin:" << fileName;
        }
    }

    qDebug() << "Loaded" << loadedCount << "UI plugins from" << searchDir;
    return loadedCount;
}
