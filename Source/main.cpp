#include <QApplication>
#include <QLoggingCategory>

#include <QAppUpdater/InstallLayout.h>

#include "AppInstance.h"

int main(int argc, char* argv[])
{
    QLoggingCategory::setFilterRules(
        QStringLiteral("qt.multimedia.ffmpeg*.info=false\n"
                       "qt.multimedia.ffmpeg*.debug=false"));

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SoundLink"));
    QApplication::setApplicationVersion(QStringLiteral(SOUNDLINK_VERSION));

    QAppUpdater::InstallLayout::removeLeftovers(QAppUpdater::InstallLayout::rootDir());

    AppInstance appInstance;
    if (!appInstance.init())
        return -1;

    return QApplication::exec();
}
