//
// Created by Stalker7274 on 21.09.2025.
//

#include "playlist.h"

#include <QDirIterator>


playlist::playlist(QObject* parent)
{

}

playlist::~playlist()
{

}

void playlist::addSong(song* NewSong)
{
    songs.append(NewSong);
}

song* playlist::next()
{
    currentSongIndex++;
    if (currentSongIndex >= songs.size()) currentSongIndex = 0;
    return songs.at(currentSongIndex);
}

song* playlist::prev()
{
    currentSongIndex--;
    if (currentSongIndex < 0) currentSongIndex = songs.size() - 1;
    return songs.at(currentSongIndex);
}

playlist* playlist::constructDir(const QString& dirPath, bool recursive)
{
    const auto createdPlaylist = new playlist();

    QDirIterator it(dirPath, {"*.mp3"}, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        song* tempSong = new song(it.next());
        createdPlaylist->addSong(tempSong);
    }

    return createdPlaylist;
}


