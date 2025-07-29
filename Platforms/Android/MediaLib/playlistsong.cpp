#include "playlistsong.h"
#include "ui_playlistsong.h"
#include <QFileInfo>

playlistSong::playlistSong(int index, QString songPath, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::playlistSong)
{
    ui->setupUi(this);

    ui->songIndexLabel->setText(QString::number(index));
    ui->songNameLabel->setText(QFileInfo(songPath).baseName());

    songIndex = index;
    path = songPath;
}

playlistSong::~playlistSong()
{
    delete ui;
}
