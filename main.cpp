
#include "AppInstance.h"
#include <QApplication>

#ifdef Q_OS_WIN
#include "mainwindow.h"
#elifdef Q_OS_ANDROID
#include "androidmainwindow.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AppInstance *w = AppInstance::getInstance();

#ifdef Q_OS_WIN

    mainWindow* win = new mainWindow(w);
    win->show();
#elifdef  Q_OS_ANDROID

    androidMainWindow* win = new androidMainWindow();
    win->show();

    //QQmlApplicationEngine engine;
    //engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    //if (engine.rootObjects().isEmpty())
      //  return -1;

#endif

    return a.exec();
}
