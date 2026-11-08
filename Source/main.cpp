#include <QApplication>

#include <QAppUpdater/InstallLayout.h>

#include "AppInstance.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SoundLink"));
    QApplication::setApplicationVersion(QStringLiteral(SOUNDLINK_VERSION));

    QAppUpdater::InstallLayout::removeLeftovers(QAppUpdater::InstallLayout::rootDir());

    AppInstance appInstance;
    if (!appInstance.init())
        return -1;

    return QApplication::exec();
}
