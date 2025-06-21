//
// Created by Stalker7274 on 18.06.2025.
//

#ifndef ADDTOPLAYLIST_H
#define ADDTOPLAYLIST_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class addToPlaylist; }
QT_END_NAMESPACE

class addToPlaylist : public QWidget {
Q_OBJECT

public:
    explicit addToPlaylist(QWidget *parent = nullptr, QString songName = "");
    ~addToPlaylist() override;

public slots:

    void playlistSelected();

private:
    Ui::addToPlaylist *ui;
};


#endif //ADDTOPLAYLIST_H
