//
// Created by Stalker7274 on 02.12.2025.
//

#ifndef SOUNDLINK_TITLEBAR_H
#define SOUNDLINK_TITLEBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();

private:
    QPoint dragPosition;
    bool isDragging = false;
    bool isMaximized = false;

    // Сохраняем нормальную геометрию для восстановления
    QRect normalGeometry;

    QPushButton *minimizeButton;
    QPushButton *maximizeButton;
    QPushButton *closeButton;
    QLabel *titleLabel;

    void setupUI();
    void toggleMaximize();
};

#endif // SOUNDLINK_TITLEBAR_H
