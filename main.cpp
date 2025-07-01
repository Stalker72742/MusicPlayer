
#include "AppInstance.h"
#include <QApplication>

#ifdef Q_OS_WIN
#include "mainwindow.h"
#endif

#ifdef Q_OS_ANDROID
#include "Platforms\Android\androidmainwindow.h"
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AppInstance *w = new AppInstance();

#ifdef Q_OS_WIN

    mainWindow* win = new mainWindow(w);

    win->show();

#endif

    androidMainWindow* win = new androidMainWindow();

    win->show();


    return a.exec();
}
