//
// Created by Stalker7274 on 03.06.2025.
//

#ifndef MEDIALIBITEM_H
#define MEDIALIBITEM_H

#include <QDir>
#include <QPushButton>
#include <QWidget>


class PlayerSubsystem;
class QLabel;

QT_BEGIN_NAMESPACE
namespace Ui { class mediaLibItem; }
QT_END_NAMESPACE

class mediaLibItem : public QWidget {
Q_OBJECT

public:
    explicit mediaLibItem(QWidget *parent = nullptr, QString playlist = "");
    ~mediaLibItem() override;

    void setFocus();

    bool eventFilter(QObject *watched, QEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

private:
    Ui::mediaLibItem *ui;

    QString playlistPath;

    QLabel* playlistLabel;

    QPushButton* playlistMenu;

    PlayerSubsystem* player;
};


#endif //MEDIALIBITEM_H
