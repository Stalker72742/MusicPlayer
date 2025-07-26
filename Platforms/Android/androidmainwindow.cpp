#include "androidmainwindow.h"

#include <QDir>
#include <QStandardPaths>

#include "AppInstanceLibs.h"
#include "AppInstance.h"
#include "Permissions/permissionhandler.h"
#include "medialibitemwidget.h"
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

    QString mediaLibFolder = AppInstanceLibs::getAppInstance(this)->getSubsystem<PlayerSubsystem>()->getMediaLibFolder();

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
