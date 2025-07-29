#include "androidmainwindow.h"
#include "MediaLib/medialibitemwidget.h"
#include "MediaLib/playlistsong.h"
#include "Permissions/permissionHandler.h"
#include "AppInstance.h"
#include <QDirIterator>
#include "AppInstanceLibs.h"
#include "ui_androidmainwindow.h"

androidMainWindow::androidMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::androidMainWindow)
{
    ui->setupUi(this);

    connect(ui->medialibButton, &QPushButton::clicked, this, &androidMainWindow::mediaButtonClicked);
    connect(ui->searchButton, &QPushButton::clicked, this, &androidMainWindow::searchButtonClicked);

    PermissionHandler* handler = PermissionHandler::instance();
    handler->requestPermissions();

    ui->rootStackedWidget->setCurrentIndex(3);
    ui->playlistSongsScroll->setLayout(new QVBoxLayout);
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

    QString mediaLibFolder = AppInstance::getInstance()->getSubsystem<PlayerSubsystem>()->getMusicFolder();

    medialibItemWidget* allSongs = new medialibItemWidget(mediaLibFolder);
    connect(allSongs, &medialibItemWidget::clicked, this, &androidMainWindow::playlistSelected);

    mediaLibScrollLayout->addWidget(allSongs);
    mediaLibScrollLayout->addStretch();
}

void androidMainWindow::playlistSelected() {

    medialibItemWidget* playlist = qobject_cast<medialibItemWidget*>(sender());
    if(!playlist) return;

    ui->rootStackedWidget->setCurrentIndex(2);
    QString playlistPath = playlist->getPlaylist();

    QDirIterator it(playlistPath, QDir::Files, QDirIterator::Subdirectories );
    int i = 0;

    while (it.hasNext()) {
        i++;
        QString songPath = it.next();

        ui->playlistSongsScroll->layout()->addWidget(new playlistSong(i, songPath, this));
    }

    qobject_cast<QVBoxLayout*>(ui->playlistSongsScroll->layout())->addStretch();
}

void androidMainWindow::searchButtonClicked(bool checked)
{
    ui->rootStackedWidget->setCurrentIndex(0);
}
