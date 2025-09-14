#include "currentplayingsong.h"
#include "../../../Core/Public/Subsystems/Player/SubObjects/song.h"
#include "AppInstance.h"
#include "PlayerSubsystem.h"
#include "ui_currentplayingsong.h"

currentPlayingSong::currentPlayingSong(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::currentPlayingSong)
{
    ui->setupUi(this);

    /*connect(ui->prevSongButton, &QPushButton::clicked, this, [](){

        AppInstance::getInstance()->getSubsystem<PlayerSubsystem>()->PreviousSong();
    });

    connect(ui->pausePlayButton, &QPushButton::clicked, this, [this](){
        PlayerSubsystem* player = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>();

        if(paused){

            player->Resume();
            paused = false;
        }else{

            player->Pause();
            paused = true;
        }
    });

    connect(ui->nextSongButton, &QPushButton::clicked, this, [](){

        AppInstance::getInstance()->getSubsystem<PlayerSubsystem>()->NextSong();
    });*/
}

currentPlayingSong::~currentPlayingSong()
{
    delete ui;
}

void currentPlayingSong::setSong(song* newSong){

    ui->authorNameLabel->setText("");
    ui->songNameLabel->setText(newSong->getName());

    currentSong = newSong;
    paused = false;
}
