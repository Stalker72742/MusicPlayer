//
// Created by Stalker7274 on 26.03.2025.
//

#ifndef SONGPATH_H
#define SONGPATH_H

#include <QJsonObject>
#include <QProcess>
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

    explicit SongPath(const QJsonObject &Json) {

        LocalPath = "";
        Url = Json["url"].toString();
        Name = Json["title"].toString();

        QProcess *process = new QProcess();

        QStringList args;

        args << "--get-url"
         << "--no-playlist"
         << "--no-warnings"
         << "--ignore-no-formats"
         << "--no-warnings"
         << "-f"
         << "bestaudio"
         << "--user-agent"
         << "Mozilla/5.0"
         << Url;

        process->setProcessChannelMode(QProcess::MergedChannels);

        process->start("yt-dlp", args);

        process->waitForFinished();

        Url = QString(process->readAllStandardOutput()).trimmed();

        qDebug() << "Done";
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
