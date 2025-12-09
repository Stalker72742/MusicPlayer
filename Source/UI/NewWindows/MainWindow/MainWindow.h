//
// Created by Stalker7274 on 02.12.2025.
//

#ifndef SOUNDLINK_MAINWINDOW_H
#define SOUNDLINK_MAINWINDOW_H

#include <QMainWindow>
#include <qvectornd.h>

class BottomBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    explicit MainWindow();
    ~MainWindow() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    const QSize DefaultSize {1200, 800};
    const int ResizeBorderWidth = 5;

private:
    enum class ResizeEdge {
        None,
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    ResizeEdge resizeEdge = ResizeEdge::None;
    QPoint resizeStartPos;
    QRect resizeStartGeometry;

    BottomBar* bottomBar;

    ResizeEdge getResizeEdge(const QPoint &pos);
    void updateCursor(const QPoint &pos);
    bool isDragging() const;
};


#endif // SOUNDLINK_MAINWINDOW_H
