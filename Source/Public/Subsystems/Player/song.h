//
// Created by Stalker7274 on 07.05.2025.
//

#pragma once

#include <QObject>

#ifndef SONG_H
#define SONG_H

class song : public QObject {
    Q_OBJECT
public:

    explicit song(QObject *parent = nullptr);

    explicit song(const QString &fileName);

    QString getName();

    QUrl getPlayerUrl();

private:

    QString getAudioStream();

    QString songName;
    QString songPathOrUrl;

    QString songAudioStream;

    bool bLocalSong;
};



#endif //SONG_H
