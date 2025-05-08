//
// Created by Stalker7274 on 12.04.2025.
//

#include "mainwindow.h"
#include "ui_MainWindow.h"

#include <QMouseEvent>
#include "ytSearcherSub.h"
#include "AppInstance.h"
#include "titlebar.h"
#include "AppInstanceLibs.h"
#include "playlistitem.h"
#include "playlistlist.h"
#include <QTimer>

mainWindow::mainWindow(QObject *Parent) :
    QMainWindow(nullptr), ui(new Ui::mainWindow) {
    ui->setupUi(this);

    parent = Parent;

    setWindowFlags(Qt::FramelessWindowHint);

    setMenuWidget(new TitleBar(this));

    AppInstance* AppInstance = AppInstanceLibs::getAppInstance(this);

    ytSearcherSub* ytSearcher = AppInstance->getSubsystem<ytSearcherSub>();

    connect(ytSearcher, &ytSearcherSub::searchResults, this, &mainWindow::addItemToPlaylist);

    playerSubsystem = AppInstance->getSubsystem<PlayerSubsystem>();

    connect(ui->playButton, &QPushButton::clicked, this, &mainWindow::playPause);
    connect(ui->nextButton, &QPushButton::clicked, this, &mainWindow::playNext);
    connect(ui->previousButton, &QPushButton::clicked, this, &mainWindow::playPrevious);
    connect(ui->mixButton, &QPushButton::clicked, this, &mainWindow::mixPlaylist);
    connect(ui->loopButton, &QPushButton::clicked, this, &mainWindow::enableLoop);

    sideMenu = new playlistList(this);

    ui->mediateka->setLayout(new QVBoxLayout());
    ui->mediateka->layout()->addWidget(sideMenu);
}

mainWindow::~mainWindow() {

    delete ui;
}

AppInstance* mainWindow::getAppInstance() const {

    return qobject_cast<AppInstance*>(parent);
}

void mainWindow::addItemToPlaylist(QJsonObject Json, ESearchResultsState searchResultsState) {

    playListItem* playlistItem;

    QLayoutItem* item;

    switch (searchResultsState) {

        case pending:

            playlistItem = new playListItem(this, Json);
            ui->playlistLayout->addWidget(playlistItem);

            qDebug() << "playlist item created";
            break;

        case startParsing:

            while ((item = ui->playlistLayout->takeAt(0)) != nullptr) {
                if (item->widget()) {
                    delete item->widget();
                }
                delete item;
            }
            qDebug() << "playlist item deleted";
            break;

        default:

            break;
    }
}

void mainWindow::playPause() {

    if (bPaused) {

        playerSubsystem->Resume();
        bPaused = false;
        
    }else {

        playerSubsystem->Pause();
        bPaused = true;
    }
}

void mainWindow::playNext() const {

    playerSubsystem->NextSong();
}

void mainWindow::playPrevious() const {

    playerSubsystem->PreviousSong();
}

void mainWindow::enableLoop() {

}

void mainWindow::mixPlaylist() {

}