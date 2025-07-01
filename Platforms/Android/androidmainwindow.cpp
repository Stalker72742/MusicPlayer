#include "androidmainwindow.h"
#include "ui_androidmainwindow.h"

androidMainWindow::androidMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::androidMainWindow)
{
    ui->setupUi(this);

    connect(ui->medialibButton, &QPushButton::clicked, this, &androidMainWindow::mediaButtonClicked);
    connect(ui->searchButton, &QPushButton::clicked, this, &androidMainWindow::searchButtonClicked);

    ui->label->setText("мвчячмячмячмячмячмямячмяч");
}

androidMainWindow::~androidMainWindow()
{
    delete ui;
}

void androidMainWindow::mediaButtonClicked(bool checked)
{

    ui->rootStackedWidget->setCurrentIndex(1);
}

void androidMainWindow::searchButtonClicked(bool checked)
{

    ui->rootStackedWidget->setCurrentIndex(0);
}
