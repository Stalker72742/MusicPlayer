

#include "AppInstance.h"
#include "PlayerSubsystem.h"

#ifdef Q_OS_WIN
#include "Source/UI/Windows/MainWindow/mainwindow.h"
#include <QApplication>
#elifdef Q_OS_ANDROID
#include "Source/UI/Android/Subsystems/androidjavaplayer.h"
#include "Source/UI/Android/androidmainwindow.h"
#include "Source/UI/Android/PermissionsHandler/permissionHandler.h"
#include <QQmlApplicationEngine>
#include <QGuiApplication>
#include <QtQuickControls2/QQuickStyle>
#include "Source/UI/Android/Widgets/Interfaces/filemanager.h"
#include <QQmlContext>
#endif

int main(int argc, char *argv[])
{


#ifdef Q_OS_WIN

    AppInstance *w = AppInstance::getInstance();

    QApplication a(argc, argv);

    mainWindow* win = new mainWindow(w);
    win->show();

#elifdef  Q_OS_ANDROID

    QGuiApplication a(argc, argv);

    //androidMainWindow* win = new androidMainWindow();
    //win->show();

    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

    AppInstance *w = AppInstance::getInstance();

    w->addSubsystem(new PlayerSubsystem( new AndroidJavaPlayer(nullptr), w));

    QQuickStyle::setStyle("Fusion");
    QQmlApplicationEngine engine;
    fileManager fileManager;
    engine.rootContext()->setContextProperty("fileManager", &fileManager);
    engine.load(QUrl(QStringLiteral("qrc:/Source/UI/Android/Widgets/AndroidMainWindow.qml")));



#endif

    return a.exec();
}
