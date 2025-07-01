#ifndef MEDIALIBITEM_H
#define MEDIALIBITEM_H

#include <QWidget>

namespace Ui {
class MedialibItem;
}

class medialibItem : public QWidget
{
    Q_OBJECT

public:
    explicit medialibItem(QWidget *parent = nullptr);
    ~medialibItem();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    Ui::MedialibItem *ui;
};

#endif // MEDIALIBITEM_H
