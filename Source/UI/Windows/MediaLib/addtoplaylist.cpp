//
// Created by Stalker7274 on 18.06.2025.
//

#include "addtoplaylist.h"

#include "AppInstance.h"
#include "PlayerSubsystem.h"
#include "ui_addToPlaylist.h"
#include <QPushButton>

addToPlaylist::addToPlaylist(QWidget *parent, QString songName) :
    QWidget(parent), ui(new Ui::addToPlaylist) {
    ui->setupUi(this);

    ui->selectPlaylistLabel->setText("Select playlist for " + songName);

    QList<QString> playlists; //SubsystemBase::GetSubsystem<PlayerSubsystem>()->getPlaylists();

    QVBoxLayout* layout = new QVBoxLayout(ui->playlistsScroll);

    foreach(QString playlist, playlists) {

        QPushButton *playlistButton = new QPushButton(playlist, this);

        playlistButton->setStyleSheet("color: rgb(255, 255, 255);");

        layout->addWidget(playlistButton);

        connect(playlistButton, &QPushButton::clicked, this, &addToPlaylist::playlistSelected);
    }
}

addToPlaylist::~addToPlaylist() {
    delete ui;
}

void addToPlaylist::playlistSelected() {

    /*AppInstanceLibs::getAppInstance(this)->getSubsystem<PlayerSubsystem>()->addSongToPlaylistByName(ui->selectPlaylistLabel->text(),
        qobject_cast<QPushButton*>(sender())->text());*/

    this->deleteLater();
}
