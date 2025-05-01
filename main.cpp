#include "mainwindow.h"

#include <QApplication>

#include "Public/Framework/AppInstance.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AppInstance w(nullptr);

    return a.exec();
}
