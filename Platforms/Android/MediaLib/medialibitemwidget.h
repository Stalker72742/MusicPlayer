//
// Created by Stalker7274 on 02.07.2025.
//

#ifndef MEDIALIBITEMWIDGET_H
#define MEDIALIBITEMWIDGET_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class medialibItemWidget; }
QT_END_NAMESPACE

class medialibItemWidget : public QWidget {
Q_OBJECT

public:
    explicit medialibItemWidget(QWidget *parent = nullptr);
    explicit medialibItemWidget(const QString& pathToPlaylist, const QString &playlistDescript = "", QWidget *parent = nullptr);
    ~medialibItemWidget() override;

    void mousePressEvent(QMouseEvent *event) override;

signals:
    void clicked(medialibItemWidget* widget);

private:
    Ui::medialibItemWidget *ui;
};


#endif //MEDIALIBITEMWIDGET_H
