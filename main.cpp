

#include "AppInstance.h"
#include "FileManager/FileManager.h"
#include "PlayerSubsystem.h"
#include "Source/UI/Android/Subsystems/QMLInerfaces/playerdata.h"
#include "Source/UI/Android/Subsystems/QMLInerfaces/playlistModel.h"
#include "staticData.h"

#ifdef Q_OS_WIN
#include "Source/UI/NewWindows/MainWindow/MainWindow.h"
#include <QApplication>
#include "windowsmediaplayer.h"
#elifdef Q_OS_ANDROID
#include "Source/UI/Android/Subsystems/androidjavaplayer.h"
#include "Source/UI/Android/Subsystems/PermissionsHandler/permissionHandler.h"
#include <QQmlApplicationEngine>
#include <QGuiApplication>
#include <QtQuickControls2/QQuickStyle>
#include <QQmlContext>
#endif

int main(int argc, char *argv[])
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
