//
// Created by Stalker7274 on 25.04.2025.
//

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QJsonObject>
#include <QWidget>

#include "../../../../Core/Public/Subsystems/Player/SubObjects/song.h"

QT_BEGIN_NAMESPACE
namespace Ui { class playListItem; }
QT_END_NAMESPACE

class playListItem : public QWidget {
Q_OBJECT

public:
    explicit playListItem(QWidget *parent = nullptr, QJsonObject info = QJsonObject());
    explicit playListItem(song* song, QWidget* parent = nullptr);
    explicit playListItem(QWidget *parent = nullptr, QString songPathLoc = "");
    ~playListItem() override;


private:
    Ui::playListItem *ui;

    bool bInPlaylist = true;

    void mousePressEvent(QMouseEvent *event) override;

    QJsonObject data;

    QString songPath;
};


#endif //PLAYLISTITEM_H
