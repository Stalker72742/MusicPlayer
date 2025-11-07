#include "playlistwidget.h"


playlistWidget::playlistWidget(const QString &filePath, QObject* parent) : QObject{parent} {

}

QString playlistWidget::fileName() const
{
    return m_fileInfo.fileName();
}

QString playlistWidget::filePath() const
{
    return m_filePath;
}

qint64 playlistWidget::fileSize() const
{
    return m_fileInfo.size();
}
