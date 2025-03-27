//
// Created by Stalker7274 on 26.03.2025.
//

#ifndef SONGPATH_H
#define SONGPATH_H
#include <qstring.h>

struct SongPath {

    QString LocalPath;

    QString Url;

    SongPath() {
        LocalPath = "";

        Url = "";
    }
};

#endif //SONGPATH_H
