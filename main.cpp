
#include "AppInstance.h"

#include <QGuiApplication>
#ifdef Q_OS_WIN
#include "Source/UI/Windows/MainWindow/mainwindow.h"
#elifdef Q_OS_ANDROID
#include "Source/UI/Android/androidmainwindow.h"
#include <QGuiApplication>
#include "Source/UI/Android/PermissionsHandler/permissionHandler.h"
#include <QQmlApplicationEngine>
#include <QtQuickControls2/QQuickStyle>
#endif

int main(int argc, char *argv[])
{
    QGuiApplication  a(argc, argv);
    AppInstance *w = AppInstance::getInstance();

#ifdef Q_OS_WIN

    mainWindow* win = new mainWindow(w);
    win->show();
#elifdef  Q_OS_ANDROID

    //androidMainWindow* win = new androidMainWindow();
    //win->show();

    QQuickStyle::setStyle("Fusion");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/Source/UI/Android/Widgets/AndroidMainWindow.qml")));

    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

#endif

    return a.exec();
}
