

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
#endif

int main(int argc, char *argv[])
{
    AppInstance *w = AppInstance::getInstance();

#ifdef Q_OS_WIN

    QApplication a(argc, argv);

    mainWindow* win = new mainWindow(w);
    win->show();

#elifdef  Q_OS_ANDROID

    QGuiApplication a(argc, argv);

    //androidMainWindow* win = new androidMainWindow();
    //win->show();

    w->addSubsystem(new PlayerSubsystem( new AndroidJavaPlayer(nullptr), w));

    QQuickStyle::setStyle("Fusion");
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/Source/UI/Android/Widgets/AndroidMainWindow.qml")));

    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

#endif

    return a.exec();
}
