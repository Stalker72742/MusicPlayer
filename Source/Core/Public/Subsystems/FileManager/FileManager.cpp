//
// Created by Stalker7274 on 05.11.2025.
//

#include "FileManager.h"
#include "staticData.h"
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>

FileManager::FileManager(QObject* Parent) : SubsystemBase(Parent)
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile AllMusicJson(path + "/" + staticData::allMediaConfigName);
    QJsonObject playlist;

    playlist["SongName"] = "Path/To/Song/Local";

    QFileInfo fileInfo(AllMusicJson);
    QDir dir = fileInfo.absoluteDir();

    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Unable to make directory:" << dir.path();
            return;
        }
    }

    if (!AllMusicJson.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Fail to save file:" << AllMusicJson.errorString();
        return;
    }

    const QJsonDocument doc(playlist);
    AllMusicJson.write(doc.toJson(QJsonDocument::Indented));
    AllMusicJson.close();

    QFileInfo allMediaInfo(path + "/" + staticData::allMediaConfigName);
    qDebug() << "AllMusicJson success at" << allMediaInfo.absoluteFilePath();
}

bool FileManager::GetAllPlaylists(QStringList& OutFoundFiles)
{
    const QString medialibFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QStringList filters;
    filters << "*.json";

    QDirIterator it(medialibFolder, filters, QDir::Files | QDir::Dirs);

    qDebug() << "Trying to find playlists in " << medialibFolder;

    while(it.hasNext()){

        const QString filePath = it.next();
        OutFoundFiles.append(filePath);

        qDebug() << "Founded playlist " << filePath;
    }

    return !OutFoundFiles.empty();
}
