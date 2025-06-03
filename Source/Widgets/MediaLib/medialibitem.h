//
// Created by Stalker7274 on 03.06.2025.
//

#ifndef MEDIALIBITEM_H
#define MEDIALIBITEM_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class mediaLibItem; }
QT_END_NAMESPACE

class mediaLibItem : public QWidget {
Q_OBJECT

public:
    explicit mediaLibItem(QString playlistPath = "", QWidget *parent = nullptr);
    ~mediaLibItem() override;

private:
    Ui::mediaLibItem *ui;

    QString playlistPath;

    QString playlistName;
};


#endif //MEDIALIBITEM_H
