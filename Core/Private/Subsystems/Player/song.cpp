//
// Created by Stalker7274 on 07.05.2025.
//

#include "song.h"

#include <qfile.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

song::song(QJsonObject JsonObj) {

    songPathOrUrl = JsonObj["url"].toString();

    songName = JsonObj["title"].toString();

    bLocalSong = false;
}

song::song(const QString &fileName) {

    if (fileName.split('.').last() == ".json") {

        QFile configFile(fileName);
        if (!configFile.open(QIODevice::ReadOnly)) return;

        QJsonObject json = QJsonDocument::fromJson(configFile.readAll()).object();

        songPathOrUrl = json["url"].toString();

        songName = json["name"].toString();

        bLocalSong = false;

    }else if (fileName.split(':').first() == "https") {

        songPathOrUrl = fileName;

        bLocalSong = false;

    }else {

        songPathOrUrl = fileName;

        songName = fileName.split('/').last();

        bLocalSong = true;
    }
}

QString song::getName() {
    return songName;
}

void song::setName(QString NewName) {
    songName = NewName;
}

QUrl song::getPlayerUrl() {
    return bLocalSong? QUrl::fromLocalFile(songPathOrUrl) : QUrl(getAudioStream());
}

QString song::getAudioStream() {

    if (!songAudioStream.isEmpty()) {
        return songAudioStream;
    }

    process = new QProcess();

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
     << songPathOrUrl;

    process->setProcessChannelMode(QProcess::MergedChannels);

    process->start("yt-dlp", args);

    connect(process, &QProcess::finished, this, [this](int exitCode) {
        if (exitCode == 0) {
            songAudioStream = QString(process->readAllStandardOutput()).trimmed();

            qDebug() << "Audion stream loaded";

            emit audioStreamLoaded(this);
        }

        process->deleteLater();
    });

    return songAudioStream;
}