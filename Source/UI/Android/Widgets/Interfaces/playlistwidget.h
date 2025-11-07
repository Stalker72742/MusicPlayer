#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include <QObject>
#include <QFileInfo>

class playlistWidget : public QObject
{
Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(qint64 fileSize READ fileSize NOTIFY fileSizeChanged)


public:
    explicit playlistWidget(const QString &filePath, QObject* parent = nullptr);

    QString fileName() const;
    QString filePath() const;
    qint64 fileSize() const;

signals:
    void fileNameChanged();
    void filePathChanged();
    void fileSizeChanged();

private:
    QString m_filePath;
    QFileInfo m_fileInfo;
};

#endif // PLAYLISTWIDGET_H
