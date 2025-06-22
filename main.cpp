
#include "AppInstance.h"
#include <QApplication>

#ifdef Q_OS_WIN
#include "mainwindow.h"
#endif


#include "Platforms\Android\androidmainwindow.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AppInstance w(nullptr);

#ifdef Q_OS_WIN

    w.createApp<mainWindow>();

#endif

    androidMainWindow* win = new androidMainWindow();

    win->show();

    return a.exec();

}
