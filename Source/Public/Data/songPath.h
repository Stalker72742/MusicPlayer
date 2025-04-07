//
// Created by Stalker7274 on 26.03.2025.
//

#ifndef SONGPATH_H
#define SONGPATH_H
#include <QJsonObject>
#include <qstring.h>

struct SongPath {

    QString LocalPath;

    QString Url;

    QString Name = "";

    SongPath() {

        LocalPath = "";

        Url = "";

    }

    explicit SongPath(const QString &LLocalPath, const QString &LUrl) {

        LocalPath = LLocalPath;

        Url = LUrl;

        Name = LLocalPath;
    }

    explicit SongPath(const QJsonObject Json) {

        LocalPath = "";

        Url = Json["url"].toString();

        Name = Json["title"].toString();
    }

    QString GetUrl() const {
        if (LocalPath.isEmpty()){
            return Url;
        }else {
            return LocalPath;
        }
    }
};

#endif //SONGPATH_H
