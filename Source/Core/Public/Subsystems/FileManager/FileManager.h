//
// Created by Stalker7274 on 05.11.2025.
//

#ifndef SOUNDLINK_FILEMANAGER_H
#define SOUNDLINK_FILEMANAGER_H
#include "SubsystemBase.h"
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>

class QFileInfo;

struct PlaylistInfo {
    QString path;
    QString name;
    int trackCount;
};

class FileManager : public SubsystemBase
{
    Q_OBJECT
public:

    FileManager(QObject* Parent);

    // Получить список всех плейлистов (пути к файлам)
    static bool GetAllPlaylists(QStringList& OutFoundFiles);

    // Получить информацию о всех плейлистах (читает с диска)
    static QVariantList GetPlaylistsInfo();

    // Получить содержимое плейлиста по имени (читает с диска)
    static QVariantList GetPlaylistTracks(const QString& playlistName);

    // Получить содержимое плейлиста по пути (читает с диска)
    static QVariantList GetPlaylistTracksByPath(const QString& playlistPath);

    // Прочитать JSON плейлиста с диска
    static QJsonObject ReadPlaylistJson(const QString& playlistPath);
};


#endif // SOUNDLINK_FILEMANAGER_H
