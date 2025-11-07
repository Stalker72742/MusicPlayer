#include "filemanager.h"
#include "playlistwidget.h"
#include <QDir>

fileManager::fileManager(QObject* parent) : QObject{parent} {

}

QList<QObject*> fileManager::fileWidgets() const
{
    return m_fileWidgets;
}

void fileManager::loadFiles(const QString &folderPath)
{
    qDeleteAll(m_fileWidgets);
    m_fileWidgets.clear();

    QDir dir(folderPath);
    QStringList files = dir.entryList(QDir::Files);

    for (const QString &fileName : files) {
        QString fullPath = dir.absoluteFilePath(fileName);
        playlistWidget *widget = new playlistWidget(fullPath, this);
        m_fileWidgets.append(widget);
    }

    emit fileWidgetsChanged();
}
