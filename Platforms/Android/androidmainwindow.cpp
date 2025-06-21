#include "androidmainwindow.h"
#include "ui_androidmainwindow.h"

androidMainWindow::androidMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::androidMainWindow)
{
    ui->setupUi(this);
}

androidMainWindow::~androidMainWindow()
{
    delete ui;
}
