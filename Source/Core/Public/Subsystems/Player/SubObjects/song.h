//
// Created by Stalker7274 on 07.05.2025.
//
#pragma once

#include <QObject>
#include <QProcess>

#ifndef SONG_H
#define SONG_H

class song : public QObject {
    Q_OBJECT
public:

    explicit song(QJsonObject JsonObj);

    explicit song(const QString &fileName);

    QString getName();

    void setName(QString NewName);

    QUrl getPlayerUrl();

    QString getSongPath() const { return songPathOrUrl;}


signals:

    void audioStreamLoaded(song* songPtr);

private:

    QString getAudioStream();

    QString songName;
    QString songPathOrUrl;

    QString songAudioStream = "";

    bool bLocalSong;

    QProcess *process;
};



#endif //SONG_H
