#include "medialibitem.h"
#include "ui_medialibitem.h"

medialibItem::medialibItem(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MedialibItem)
{
    ui->setupUi(this);
}

medialibItem::~medialibItem()
{
    delete ui;
}

void medialibItem::mousePressEvent(QMouseEvent *event){
    QWidget::mousePressEvent(event);

}

void medialibItem::mouseMoveEvent(QMouseEvent *event){
    QWidget::mouseMoveEvent(event);

    if(0){

    }
}
