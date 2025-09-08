//
// Created by Stalker7274 on 12.04.2025.
//

#include "mainwindow.h"
#include "ui_MainWindow.h"

#include <QMouseEvent>
//#include "ytSearcherSub.h"
#include "AppInstance.h"
#include "Components/titlebar.h"
#include "PlayerSubsystem.h"
#include "SubsystemBase.h"
#include "medialibitem.h"
#include "playlistitem.h"
#include "ytSearcher.h"

#include <QTimer>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

    AppInstance* AppInstance = AppInstance::getInstance();

    //connect(AppInstance->getSubsystem<ytSearcherSub>(), &ytSearcherSub::searchResults, this, &mainWindow::addItemToPlaylist);

    playerSubsystem = SubsystemBase::GetSubsystem<PlayerSubsystem>();

    connect(playerSubsystem, &PlayerSubsystem::playlistChanged, this, &mainWindow::updatePlaylistItems);
    connect(playerSubsystem, &PlayerSubsystem::updateMusicDuration, this, &mainWindow::updateTimeSlider);
    connect(playerSubsystem, &PlayerSubsystem::onShowMediaLib, this, &mainWindow::drawMediaLib);

    updatePlaylistItems();

    connect(ui->playButton, &QPushButton::clicked, this, &mainWindow::playPause);
    connect(ui->nextButton, &QPushButton::clicked, this, &mainWindow::playNext);
    connect(ui->previousButton, &QPushButton::clicked, this, &mainWindow::playPrevious);
    connect(ui->createPlaylistButton, &QPushButton::clicked, this, &mainWindow::createPlaylist);

    connect(ui->soundSlider, &QSlider::valueChanged, playerSubsystem, &PlayerSubsystem::SetVolume);

    ui->soundSlider->setValue(playerSubsystem->getVolume());

    ui->mediatekaScrollArea->setLayout(new QVBoxLayout());

    qobject_cast<QVBoxLayout*>(ui->mediatekaScrollArea->layout())->setContentsMargins(30, 0, 0, 0);

    ui->mediatekaScrollArea->layout()->addWidget(new mediaLibItem(this, true));

    foreach(QString x, playerSubsystem->getPlaylists()) {

        auto playlist = new mediaLibItem(this, x);

        playlist->setFixedHeight(45);
        playlist->setFocus();

        ui->mediatekaScrollArea->layout()->addWidget(playlist);
    }

    qobject_cast<QVBoxLayout*>(ui->mediatekaScrollArea->layout())->addStretch();

    RegisterHotKey(reinterpret_cast<HWND>(winId()), 1, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
    RegisterHotKey(reinterpret_cast<HWND>(winId()), 2, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
    RegisterHotKey(reinterpret_cast<HWND>(winId()), 3, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
}

mainWindow::~mainWindow() {

    delete ui;
}

AppInstance* mainWindow::getAppInstance() const {

    return qobject_cast<AppInstance*>(parent);
}

bool mainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        if (const MSG* msg = static_cast<MSG*>(message); msg->message == WM_HOTKEY) {

            switch (static_cast<int>(msg->wParam)) {
                case 1:

                    playPause();
                    break;
                case 2:

                    playNext();
                    break;

                case 3:

                    playPrevious();
                    break;

                default:

                    break;
            }
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

/*void mainWindow::addItemToPlaylist(QJsonObject Json, ESearchResultsState searchResultsState) {

    playListItem* playlistItem;

    QLayoutItem* item;

    switch (searchResultsState) {

        case pending:

            playlistItem = new playListItem(this, std::move(Json));
            ui->playlistScrollArea->layout()->addWidget(playlistItem);

            qobject_cast<QVBoxLayout*>(ui->playlistScrollArea->layout())->addStretch();

            qDebug() << "playlist item created";
            break;

        case startParsing:

            while ((item = ui->playlistScrollArea->layout()->takeAt(0)) != nullptr) {
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
}*/

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

void mainWindow::createPlaylist() {

    const auto playlist = new mediaLibItem(this, "");

    playlist->setFixedHeight(45);

    playlist->setFocus();

    ui->mediatekaScrollArea->layout()->addWidget(playlist);

    qobject_cast<QVBoxLayout*>(ui->mediatekaScrollArea->layout())->addStretch();
}

void mainWindow::updatePlaylistItems() {
    QLayoutItem* item;

    ui->playlistScrollArea->setLayout(new QVBoxLayout());

    while ((item = ui->playlistScrollArea->layout()->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    foreach(song* x, playerSubsystem->getSongs()) {

        auto *playlistItem = new playListItem(x, this);
        ui->playlistScrollArea->layout()->addWidget(playlistItem);
    }

    qobject_cast<QVBoxLayout*>(ui->playlistScrollArea->layout())->addStretch(0);
    qobject_cast<QVBoxLayout*>(ui->playlistScrollArea->layout())->setSpacing(0);
}

void mainWindow::updateTimeSlider(int time) const {

    const qint64 maxDuration = playerSubsystem->getMaxDuration() / 1000;

    ui->durationSlider->setValue(static_cast<int>(static_cast<float>(time) / static_cast<float>(maxDuration) * 100.0));

    const qint64 minutes = time / 60;
    time = time % 60;

    const QString str = QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(time, 2, 10, QChar('0'));

    ui->currentDurationLabel->setText(str);
}

void mainWindow::drawMediaLib(QList<QString> songs) {

    QLayoutItem* item;

    ui->playlistScrollArea->setLayout(new QVBoxLayout());

    while ((item = ui->playlistScrollArea->layout()->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    foreach(QString songPath, songs) {

        auto *playlistItem = new playListItem(this, songPath);
        ui->playlistScrollArea->layout()->addWidget(playlistItem);
    }

    qobject_cast<QVBoxLayout*>(ui->playlistScrollArea->layout())->addStretch(0);
    qobject_cast<QVBoxLayout*>(ui->playlistScrollArea->layout())->setSpacing(0);
}
