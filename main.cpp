
#include <QApplication>
#include "AppInstance.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AppInstance w(nullptr);

    return a.exec();
}