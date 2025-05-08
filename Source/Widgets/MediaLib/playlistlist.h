//
// Created by Stalker7274 on 05.05.2025.
//

#ifndef PLAYLISTLIST_H
#define PLAYLISTLIST_H

#include <QPushButton>
#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class playlistList; }
QT_END_NAMESPACE

class playlistList : public QWidget {
Q_OBJECT

public:
    explicit playlistList(QWidget *parent = nullptr);
    ~playlistList() override;

    QPushButton *openCloseMediaLib;


public slots:

    void openClose();

private:
    Ui::playlistList *ui;

    bool bClosed = true;

    const int closedWidth = 100;

    const int openedWidth = 350;
};


#endif //PLAYLISTLIST_H
