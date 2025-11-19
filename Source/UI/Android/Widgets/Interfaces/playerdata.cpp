#include "playerdata.h"
#include "AppInstance.h"
#include "PlayerSubsystem.h"
#include "SubObjects/playlist.h"

playerData::playerData(QObject* parent) : QObject{parent}
{
    if(auto appInstance = AppInstance::getInstance()){

        player = appInstance->getSubsystem<PlayerSubsystem>();

        if(player){

            connect(player, &PlayerSubsystem::OnSongChanged, this, &playerData::OnPlayerSongChanged);

            if(playlist* currentPlaylist = player->getCurrentPlaylist()){

                if (auto song = currentPlaylist->getCurrentSong())
                {
                    setCurrentTrack(song->getName());
                    setCurrentArtist("Unknown");
                }
            }
        }
    }
}

void playerData::play()
{
    player->Resume();
}

void playerData::pause()
{
    player->Pause();
}

void playerData::next()
{
    player->NextSong();
}
void playerData::playPause()
{
    player->playPause();
}

void playerData::previous()
{
    player->PreviousSong();
}

void playerData::SetHasPlaylist(bool InHasPlaylist)
{

}

void playerData::setCurrentTrack(const QString &track)
{
    m_currentTrack = track;
    emit currentTrackChanged();

    qDebug() << "Current track: " << m_currentTrack;
}

void playerData::setCurrentArtist(const QString &artist)
{
    m_currentArtist = artist;
    emit currentArtistChanged();

    qDebug() << "Current artist: " << m_currentArtist;
}

void playerData::OnPlayerSongChanged(song* InSong)
{
    if (InSong)
    {
        setCurrentTrack(InSong->getName());
    }
}
