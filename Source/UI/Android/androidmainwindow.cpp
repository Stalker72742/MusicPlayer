#include "androidmainwindow.h"
#include "MediaLib/medialibitemwidget.h"
#include "MediaLib/playlistsong.h"
#include "CurrentPlayingSong/currentplayingsong.h"
#include "PermissionsHandler/permissionHandler.h"
#include "MediaKeyHandler/mediakeyhandler.h"
#include "AppInstance.h"
#include <QDirIterator>
#include "ui_androidmainwindow.h"
#include "ytSearcher.h"
#include <QStandardPaths>
#include <QShortcut>

androidMainWindow::androidMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::androidMainWindow)
{
    ui->setupUi(this);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    connect(ui->medialibButton, &QPushButton::clicked, this, &androidMainWindow::mediaButtonClicked);
    connect(ui->searchButton, &QPushButton::clicked, this, &androidMainWindow::searchButtonClicked);

    connect(ui->searchLineEdit, &QLineEdit::editingFinished, this, &androidMainWindow::searchLineEditFinished);

    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

    ui->rootStackedWidget->setCurrentIndex(0);
    ui->playlistSongsScroll->setLayout(new QVBoxLayout);

    //connect(AppInstance::getInstance()->getSubsystem<PlayerSubsystem>(), &PlayerSubsystem::playingSongChanged, this, &androidMainWindow::playingNewSong);
}

androidMainWindow::~androidMainWindow()
{
    delete ui;
}

void androidMainWindow::mediaButtonClicked(bool checked)
{
    ui->rootStackedWidget->setCurrentIndex(1);

    mediaLibScrollLayout = new QVBoxLayout(this);
    ui->medialibScroll->setLayout(mediaLibScrollLayout);

    QString mediaLibFolder;// = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>()->getMusicFolder();

    medialibItemWidget* allSongs = new medialibItemWidget(mediaLibFolder);
    connect(allSongs, &medialibItemWidget::clicked, this, &androidMainWindow::playlistSelected);

    mediaLibScrollLayout->addWidget(allSongs);
    mediaLibScrollLayout->addStretch();
}

void androidMainWindow::playlistSelected() {

    medialibItemWidget* playlist = qobject_cast<medialibItemWidget*>(sender());
    if(!playlist) return;

    ui->rootStackedWidget->setCurrentIndex(2);
    currentPlaylistPath = playlist->getPlaylist();

    QDirIterator it(currentPlaylistPath, {"*.mp3"}, QDir::Files, QDirIterator::Subdirectories);
    int i = 0;

    if(!ui->widget->layout()){
        ui->widget->setLayout(new QVBoxLayout());
    }

    while (it.hasNext()) {
        i++;
        QString songPath = it.next();

        playlistSong* songWidget = new playlistSong(i, songPath, this);
        songWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        connect(songWidget, &playlistSong::clicked, this, &androidMainWindow::songClicked);

        ui->widget->layout()->addWidget(songWidget);
    }

    qobject_cast<QVBoxLayout*>(ui->playlistSongsScroll->layout())->addStretch();
}

void androidMainWindow::songClicked(){

    //PlayerSubsystem* player = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>();

    /*player->setCurrentPlaylist(currentPlaylistPath);
    player->startPlayFromIndex(qobject_cast<playlistSong*>(sender())->getIndex() - 1);*/
}

void androidMainWindow::playingNewSong(song* song){

    if(!currentSongWidget){
        currentSongWidget = new currentPlayingSong(this);

        currentSongWidget->setGeometry(0, ui->rootStackedWidget->geometry().height() - 83, geometry().width(), 83);
        currentSongWidget->show();
        currentSongWidget->raise();

        qDebug() << "current song widget created at: " << currentSongWidget->geometry();
    }

    currentSongWidget->setGeometry(0, ui->rootStackedWidget->geometry().height() - 83, geometry().width(), 83);
    currentSongWidget->show();
    currentSongWidget->raise();

    currentSongWidget->setSong(song);
}

void androidMainWindow::searchButtonClicked(bool checked)
{
    ui->rootStackedWidget->setCurrentIndex(0);
}

void androidMainWindow::searchLineEditFinished(){

    qDebug() << "Searching: " << ui->searchLineEdit->text();

    /*AppInstance::getInstance()->getSubsystem<ytSearcherSub>()->download(QUrl("https://www.youtube.com/watch?v=QIZ6xYx_S-M"),
                                                                        "/storage/emulated/0/Music");*/
}
