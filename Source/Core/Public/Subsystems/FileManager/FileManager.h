//
// Created by Stalker7274 on 05.11.2025.
//

#ifndef SOUNDLINK_FILEMANAGER_H
#define SOUNDLINK_FILEMANAGER_H
#include "SubsystemBase.h"


class QFileInfo;

class FileManager : public SubsystemBase
{
    Q_OBJECT
public:

    FileManager(QObject* Parent);


    static bool GetAllPlaylists(QStringList& OutFoundFiles);
};


#endif // SOUNDLINK_FILEMANAGER_H
