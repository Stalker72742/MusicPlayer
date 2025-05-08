//
// Created by Stalker7274 on 05.05.2025.
//

#include "playlistlist.h"
#include "ui_playlistList.h"
#include <QPushButton>

playlistList::playlistList(QWidget *parent) :
    QWidget(parent), ui(new Ui::playlistList) {
    ui->setupUi(this);

    openCloseMediaLib = new QPushButton();

    openCloseMediaLib->setStyleSheet("background-color: rgb(0, 0, 0);");

    openCloseMediaLib->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    openCloseMediaLib->setFixedSize(50, 30);

    ui->layoutItems->addWidget(openCloseMediaLib);

    ui->layoutItems->addStretch();

    connect(openCloseMediaLib, SIGNAL(clicked()), this, SLOT(openClose()));
}

playlistList::~playlistList() {
    delete ui;
}

void playlistList::openClose() {

    bClosed = !bClosed;

    setFixedWidth(bClosed ? closedWidth : openedWidth);
}