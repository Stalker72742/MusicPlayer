//
// Created by Stalker7274 on 21.09.2025.
//
#pragma once

#include <QObject>

#include "song.h"

#ifndef SOUNDLINK_PLAYLIST_H
#define SOUNDLINK_PLAYLIST_H

class playlist : public QObject {
    Q_OBJECT
public:
    explicit playlist(QObject *parent = nullptr);
    ~playlist() override;

    void addSong(song* NewSong);
    QList<song*> getSongs() { return songs; }

    song* next();
    song* prev();


    static playlist* constructDir(const QString& dirPath, bool recursive = true);

protected:

    QList<song*> songs;
    qint32 currentSongIndex {0};
};


#endif // SOUNDLINK_PLAYLIST_H
