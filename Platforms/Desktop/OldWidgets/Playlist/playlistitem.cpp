//
// Created by Stalker7274 on 25.04.2025.
//

#include "playlistitem.h"
#include "ui_playListItem.h"

#include <QJsonObject>
#include <QMouseEvent>

#include "addtoplaylist.h"
#include "PlayerSubsystem.h"
#include "song.h"
#include "AppInstance.h"
#include "AppInstanceLibs.h"


playListItem::playListItem(QWidget *parent, QJsonObject info) :
    QWidget(parent), ui(new Ui::playListItem) {
    ui->setupUi(this);

    int totalSeconds = info["duration"].toInt();
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    QString timeString = QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));

    ui->time->setText(timeString);
    ui->songName->setText(info["title"].toString());

    bInPlaylist = false;

    data = info;
}

playListItem::playListItem(song *song, QWidget *parent) :
    QWidget(parent), ui(new Ui::playListItem){
    ui->setupUi(this);

    bInPlaylist = true;

    ui->songName->setText(song->getName());
}

playListItem::playListItem(QWidget *parent, QString songPathLoc):
    QWidget(parent), ui(new Ui::playListItem){
    ui->setupUi(this);

    bInPlaylist = false;
    ui->songName->setText(songPathLoc.split("/").last().split(".").first());

    songPath = songPathLoc;
}

playListItem::~playListItem() {
    delete ui;
}

void playListItem::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);

    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (bInPlaylist or !songPath.isEmpty()) {

        addToPlaylist* widget = new addToPlaylist(parentWidget(), songPath);

        widget->show();
        widget->raise();

        widget->setGeometry(88, 175, 550, 350);

    }else {

        AppInstanceLibs::getAppInstance(this)->getSubsystem<PlayerSubsystem>()->addSong(new song(data));
    }

}