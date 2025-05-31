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
#include <QTimer>

mainWindow::mainWindow(QObject *Parent) :
    QMainWindow(nullptr), ui(new Ui::mainWindow) {
    ui->setupUi(this);

    parent = Parent;

    setWindowFlags(Qt::FramelessWindowHint);

    QWidget* titleBar = new TitleBar(this);

    setMenuWidget(titleBar);

    titleBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    titleBar->setFixedHeight(60);

    titleBar->setObjectName("TitleBar");

    QTimer::singleShot(500,[titleBar] {

        titleBar->setStyleSheet("QWidget#TitleBar {"
        "border-top: none;"
        "border-left: none;"
        "border-right: none;"
        "border-bottom: 15px solid white;"
        "background-color: rgba(33, 33, 33, 255);"
        "}"
    );

    });

    AppInstance* AppInstance = AppInstanceLibs::getAppInstance(this);

    ytSearcherSub* ytSearcher = AppInstance->getSubsystem<ytSearcherSub>();

    connect(ytSearcher, &ytSearcherSub::searchResults, this, &mainWindow::addItemToPlaylist);

    playerSubsystem = AppInstance->getSubsystem<PlayerSubsystem>();

    connect(ui->playButton, &QPushButton::clicked, this, &mainWindow::playPause);
    connect(ui->nextButton, &QPushButton::clicked, this, &mainWindow::playNext);
    connect(ui->previousButton, &QPushButton::clicked, this, &mainWindow::playPrevious);
    
    playListItem* playlistItem;

    QLayoutItem* item;

    while ((item = ui->playlistLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    foreach(song* x, playerSubsystem->getSongs()) {

        playlistItem = new playListItem(x, this);
        ui->playlistLayout->addWidget(playlistItem);
    }
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