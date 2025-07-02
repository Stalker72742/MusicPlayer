#include "androidmainwindow.h"

#include "medialibitemwidget.h"
#include "ui_androidmainwindow.h"

androidMainWindow::androidMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::androidMainWindow)
{
    ui->setupUi(this);

    connect(ui->medialibButton, &QPushButton::clicked, this, &androidMainWindow::mediaButtonClicked);
    connect(ui->searchButton, &QPushButton::clicked, this, &androidMainWindow::searchButtonClicked);

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

    mediaLibScrollLayout->addWidget(new medialibItemWidget("Library", "All songs : 257 songs"));
    mediaLibScrollLayout->addWidget(new medialibItemWidget("Rock", "Playlist : 15 songs"));
    mediaLibScrollLayout->addWidget(new medialibItemWidget("Cock", "Playlist : 52 songs"));

    mediaLibScrollLayout->addStretch();
}

void androidMainWindow::searchButtonClicked(bool checked)
{

    ui->rootStackedWidget->setCurrentIndex(0);
}
