#include "androidmainwindow.h"
#include "MediaLib/medialibitemwidget.h"
#include "Permissions/permissionHandler.h"
#include "AppInstance.h"
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

}

void androidMainWindow::searchButtonClicked(bool checked)
{

    ui->rootStackedWidget->setCurrentIndex(0);
}
