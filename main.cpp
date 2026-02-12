
#include "AppInstance.h"
#include "UIPluginSystem.h"
#include "FileManager/FileManager.h"
#include "PlayerSubsystem.h"
#include "staticData.h"

// Platform-specific includes (subsystems only - NO UI!)
#ifdef Q_OS_WIN
    #include <QApplication>
    //#include "windowsmediaplayer.h"
#elifdef Q_OS_ANDROID
    #include "androidjavaplayer.h"
    #include "permissionHandler.h"
    #include "playerdata.h"
    #include "playlistModel.h"
    #include <QQmlApplicationEngine>
    #include <QGuiApplication>
    #include <QtQuickControls2/QQuickStyle>
    #include <QQmlContext>
#endif

// ============================================================================
// ZERO-DEPENDENCY ARCHITECTURE - No direct UI includes!
// UI modules are loaded through UIPluginSystem using factory pattern
// ============================================================================

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    QApplication app(argc, argv);

    // Get AppInstance singleton
    AppInstance* appInstance = AppInstance::getInstance();

    // Initialize subsystems
    //appInstance->addSubsystem(new PlayerSubsystem(new WindowsMediaPlayer(nullptr)));

    // Initialize UI Plugin System
    appInstance->initializeUISystem();

    // Create UI from config/registry (zero dependencies!)
    // Factories are auto-registered via UI_AUTO_REGISTER macro
    appInstance->createUIFromConfig("NewWindows");  // Auto-selects best UI for platform!

#elifdef Q_OS_ANDROID
    QGuiApplication app(argc, argv);

    // Request permissions first
    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

    // Get AppInstance singleton
    AppInstance* appInstance = AppInstance::getInstance();

    // Initialize subsystems
    appInstance->addSubsystem(new staticData(nullptr));
    appInstance->addSubsystem(new FileManager(nullptr));
    appInstance->addSubsystem(new PlayerSubsystem(new AndroidJavaPlayer(nullptr), appInstance));

    // Initialize QML models
    PlaylistModel playlistModel(nullptr);
    playerData playerDataModel(nullptr);

    // Setup QML engine
    QQuickStyle::setStyle("Fusion");
    QQmlApplicationEngine engine;

    // Register context properties
    engine.rootContext()->setContextProperty("playlistModel", &playlistModel);
    engine.rootContext()->setContextProperty("playerData", &playerDataModel);

    // Load QML
    engine.load(QUrl(QStringLiteral("qrc:/MainWindow/androidMainWindow.qml")));

#endif

    return app.exec();
}


// ============================================================================
// OLD ARCHITECTURE - Commented out for reference
// ============================================================================
/*
int main_old(int argc, char *argv[])
{
#ifdef Q_OS_WIN

    QApplication a(argc, argv);

    AppInstance *w = AppInstance::getInstance();
    w->addSubsystem(new PlayerSubsystem(new WindowsMediaPlayer(nullptr)));

    MainWindow* win = new MainWindow();
    win->show();

#elifdef  Q_OS_ANDROID

    QGuiApplication a(argc, argv);

    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

    AppInstance *w = AppInstance::getInstance();
    w->addSubsystem(new staticData(nullptr));
    w->addSubsystem(new FileManager(nullptr));
    w->addSubsystem(new PlayerSubsystem( new AndroidJavaPlayer(nullptr), w));

    PlaylistModel playlistModel(nullptr);
    playerData playerData(nullptr);

    QQuickStyle::setStyle("Fusion");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("playlistModel", &playlistModel);
    engine.rootContext()->setContextProperty("playerData", &playerData);
    engine.load(QUrl(QStringLiteral("qrc:/MainWindow/androidMainWindow.qml")));

#endif

    return a.exec();
}
*/
