
// ============================================================================
// EXAMPLE: Using UI Plugin System (alternative to main.cpp)
// ============================================================================

#include "AppInstance.h"
#include "UIPluginSystem.h"
#include "FileManager/FileManager.h"
#include "PlayerSubsystem.h"
#include "staticData.h"

#ifdef Q_OS_WIN
    #include <QApplication>
    #include "windowsmediaplayer.h"
    #include "MainWindow.h"  // NewWindows/MainWindow
    #include "mainwindow.h"  // Windows/mainwindow (old)
#elifdef Q_OS_ANDROID
    #include "androidjavaplayer.h"
    #include "permissionHandler.h"
    #include <QQmlApplicationEngine>
    #include <QGuiApplication>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif

    // Get AppInstance singleton
    AppInstance* appInstance = AppInstance::getInstance();

    // Initialize subsystems
#ifdef Q_OS_WIN
    appInstance->addSubsystem(new PlayerSubsystem(new WindowsMediaPlayer(nullptr)));
#elifdef Q_OS_ANDROID
    PermissionHandler::instance()->requestPermissions();
    appInstance->addSubsystem(new staticData(nullptr));
    appInstance->addSubsystem(new FileManager(nullptr));
    appInstance->addSubsystem(new PlayerSubsystem(new AndroidJavaPlayer(nullptr), appInstance));
#endif

    // ========================================================================
    // METHOD 1: Use traditional template approach (current approach)
    // ========================================================================
    #ifdef USE_TRADITIONAL_UI
        #ifdef Q_OS_WIN
            #ifdef USE_NEW_UI
                appInstance->createApp<MainWindow>();
            #else
                appInstance->createApp<mainWindow>();
            #endif
        #endif
    #endif

    // ========================================================================
    // METHOD 2: Use UI Plugin System with config file
    // ========================================================================
    #ifdef USE_UI_PLUGINS
        // Initialize UI system
        appInstance->initializeUISystem(true);

        // Register available UI factories (for static linking)
        UIPluginLoader* uiLoader = appInstance->getUILoader();
        #ifdef Q_OS_WIN
            REGISTER_UI_MODULE(uiLoader, "NewWindows", MainWindow);
            REGISTER_UI_MODULE(uiLoader, "OldWindows", mainWindow);
        #endif

        // Create UI from config (automatically selects best for platform)
        appInstance->createUIFromConfig();

        // OR create specific UI:
        // appInstance->createUIFromConfig("NewWindows");
    #endif

    // ========================================================================
    // METHOD 3: Programmatic UI selection without config file
    // ========================================================================
    #ifdef USE_PROGRAMMATIC_UI
        appInstance->initializeUISystem(false);
        UIPluginLoader* uiLoader = appInstance->getUILoader();

        // Create UI configs programmatically
        UIConfig newUIConfig;
        newUIConfig.name = "NewWindows";
        newUIConfig.platform = "windows";
        newUIConfig.priority = 100;
        newUIConfig.enabled = true;
        newUIConfig.factory = []() -> QWidget* { return new MainWindow(); };
        uiLoader->addUIConfig(newUIConfig);

        UIConfig oldUIConfig;
        oldUIConfig.name = "OldWindows";
        oldUIConfig.platform = "windows";
        oldUIConfig.priority = 50;
        oldUIConfig.enabled = false;  // Disabled by default
        oldUIConfig.factory = []() -> QWidget* { return new mainWindow(); };
        uiLoader->addUIConfig(oldUIConfig);

        // Create UI
        appInstance->createUIFromConfig();
    #endif

    return app.exec();
}

// ============================================================================
// Example ui_config.json structure:
// ============================================================================
/*
{
  "ui_modules": [
    {
      "name": "NewWindows",
      "platform": "windows",
      "main_window_class": "MainWindow",
      "priority": 100,
      "enabled": true,
      "settings": {
        "theme": "dark",
        "frameless": true
      }
    },
    {
      "name": "OldWindows",
      "platform": "windows",
      "main_window_class": "mainWindow",
      "priority": 50,
      "enabled": false
    }
  ]
}
*/
