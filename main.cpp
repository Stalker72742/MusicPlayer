
#include <QApplication>
#include "AppInstance.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AppInstance w(nullptr);

    w.createApp<mainWindow>();

    return a.exec();
}
