//
// Created by Stalker7274 on 07.05.2025.
//

#include "song.h"

#include <qfile.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

song::song(QObject *parent) {

}

song::song(const QString &fileName) {

    if (fileName.split('.').last() == ".json") {

        QFile configFile(fileName);
        if (!configFile.open(QIODevice::ReadOnly)) return;

        QJsonObject json = QJsonDocument::fromJson(configFile.readAll()).object();

        songPathOrUrl = json["url"].toString();

        songName = json["name"].toString();

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

QUrl song::getPlayerUrl() {
    return bLocalSong? QUrl(songPathOrUrl) : QUrl(getAudioStream());
}

QString song::getAudioStream() {
    
}