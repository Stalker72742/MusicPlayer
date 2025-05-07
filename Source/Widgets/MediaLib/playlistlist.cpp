//
// Created by Stalker7274 on 05.05.2025.
//

#include "playlistlist.h"
#include "ui_playlistList.h"
#include <QPushButton>

playlistList::playlistList(QWidget *parent) :
    QWidget(parent), ui(new Ui::playlistList) {
    ui->setupUi(this);

    OpenPlaylistsButton= new QPushButton("b");

    ui->layoutItems->addWidget(OpenPlaylistsButton);

    ui->layoutItems->addStretch();
}

playlistList::~playlistList() {
    delete ui;
}
