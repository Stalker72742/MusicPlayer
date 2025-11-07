#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>

class fileManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<QObject*> fileWidgets READ fileWidgets NOTIFY fileWidgetsChanged)


public:
    explicit fileManager(QObject* parent = nullptr);
    QList<QObject*> fileWidgets() const;

    Q_INVOKABLE void loadFiles(const QString &folderPath);

signals:
    void fileWidgetsChanged();

private:
    QList<QObject*> m_fileWidgets;
};

#endif // FILEMANAGER_H
