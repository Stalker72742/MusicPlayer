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

    QStringList filters;
    filters << "*.mp3";

    QDirIterator it(staticData::DefaultMusicFolder, filters, QDir::Files | QDir::Dirs);

    while(it.hasNext()){
        QString file = it.next();

        QFileInfo fileInfo(file);

        playlist[fileInfo.baseName()] = file;

        qDebug() << "Added " << fileInfo.baseName() << " " << file << " to all songs playlist";
    }

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

QJsonObject FileManager::ReadPlaylistJson(const QString& playlistPath)
{
    QFile file(playlistPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open playlist file:" << playlistPath;
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON in playlist:" << playlistPath;
        return QJsonObject();
    }

    return doc.object();
}

QVariantList FileManager::GetPlaylistsInfo()
{
    QVariantList result;
    QStringList playlistFiles;

    if (!GetAllPlaylists(playlistFiles)) {
        return result;
    }

    for (const QString& filePath : playlistFiles) {
        QJsonObject json = ReadPlaylistJson(filePath);

        QVariantMap info;
        info["path"] = filePath;
        info["name"] = QFileInfo(filePath).baseName();
        info["trackCount"] = json.size();

        result.append(info);
    }

    return result;
}

QVariantList FileManager::GetPlaylistTracks(const QString& playlistName)
{
    QStringList playlistFiles;
    if (!GetAllPlaylists(playlistFiles)) {
        return QVariantList();
    }

    qDebug() << "Trying find playlist " << playlistName << " in " << playlistFiles;

    for (const QString& filePath : playlistFiles) {
        if (QFileInfo(filePath).baseName() == playlistName) {
            return GetPlaylistTracksByPath(filePath);
        }
    }

    return QVariantList();
}

QVariantList FileManager::GetPlaylistTracksByPath(const QString& playlistPath)
{
    QVariantList result;
    QJsonObject json = ReadPlaylistJson(playlistPath);

    // JSON формат: { "Song Name": "/path/to/song.mp3", ... }
    for (auto it = json.begin(); it != json.end(); ++it) {
        QVariantMap track;
        track["title"] = it.key();
        track["path"] = it.value().toString();
        result.append(track);

        qDebug() << "Founded song " << track;
    }

    return result;
}
