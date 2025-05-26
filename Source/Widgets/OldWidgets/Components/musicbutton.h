//
// Created by Stalker7274 on 22.03.2025.
//

#ifndef MUSICBUTTON_H
#define MUSICBUTTON_H

#include <QPushButton>
#include "songPath.h"

class MusicButton : public QWidget {
Q_OBJECT

public:
    explicit MusicButton(QWidget *parent);
    explicit MusicButton(QWidget *parent, QString musicPathLoc);
    explicit MusicButton(QWidget *parent, QJsonObject video);
    ~MusicButton() override;

signals:

    void SignalClicked(SongPath song);

private:

    QPushButton *button;

    QJsonObject videoInfo;

    bool bHasLocalFile = false;

    bool bInited = false;

    SongPath musicPath;

private slots:
    void OnClicked();
};


#endif //MUSICBUTTON_H
