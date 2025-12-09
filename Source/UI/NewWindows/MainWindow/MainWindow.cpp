//
// Created by Stalker7274 on 02.12.2025.
//

#include "MainWindow.h"
#include "../TitleBar/TitleBar.h"
#include "../BottomBar/BottomBar.h"

#include <QVBoxLayout>
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

MainWindow::MainWindow() : QMainWindow(nullptr)
{
    // Отключаем Windows DPI scaling
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    resize(DefaultSize);

    //Style
    setStyleSheet("background-color: rgb(0, 0, 0);");

    //Widgets
    auto centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto rootVerticalLayout = new QVBoxLayout(centralWidget);
    rootVerticalLayout->setContentsMargins(0, 0, 0, 0);
    rootVerticalLayout->setSpacing(0);

    // Title bar
    auto titleBar = new TitleBar(this);
    titleBar->setStyleSheet("background-color: #1a1a1a;");
    rootVerticalLayout->addWidget(titleBar);

    auto middleHorizontalLayout = new QHBoxLayout();

    auto leftPanelWidget = new QWidget();
    auto mainPanelWidget = new QWidget();
    auto bottomPanelWidget = new QWidget();

    leftPanelWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    mainPanelWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    bottomPanelWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    leftPanelWidget->setMinimumWidth(250);
    bottomPanelWidget->setMinimumHeight(60);

    rootVerticalLayout->addLayout(middleHorizontalLayout);

    middleHorizontalLayout->addSpacerItem(new QSpacerItem(5, 5, QSizePolicy::Fixed));
    middleHorizontalLayout->addWidget(leftPanelWidget);
    middleHorizontalLayout->addSpacerItem(new QSpacerItem(17, 5, QSizePolicy::Fixed));
    middleHorizontalLayout->addWidget(mainPanelWidget);
    middleHorizontalLayout->addSpacerItem(new QSpacerItem(5, 5, QSizePolicy::Fixed));

    rootVerticalLayout->addSpacerItem(new QSpacerItem(1, 5, QSizePolicy::Fixed));
    rootVerticalLayout->addWidget(bottomPanelWidget);

    leftPanelWidget->setStyleSheet("background-color: #252525;");
    mainPanelWidget->setStyleSheet("background-color: #353535;");
    //bottomPanelWidget->setStyleSheet("background-color: rgba(0, 0, 0, 0);");

    bottomBar = new BottomBar(this);

    auto bottomPanelLayout = new QVBoxLayout();
    bottomPanelWidget->setLayout(bottomPanelLayout);

    bottomPanelLayout->addWidget(bottomBar);

    bottomBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setMouseTracking(true);
    installEventFilter(this);

#ifdef Q_OS_WIN
    // Регистрируем горячие клавиши для мультимедиа
    RegisterHotKey(reinterpret_cast<HWND>(winId()), 1, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
    RegisterHotKey(reinterpret_cast<HWND>(winId()), 2, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
    RegisterHotKey(reinterpret_cast<HWND>(winId()), 3, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
#endif
}

MainWindow::~MainWindow()
{

}

MainWindow::ResizeEdge MainWindow::getResizeEdge(const QPoint &pos)
{
    const int bw = ResizeBorderWidth;
    const int w = width();
    const int h = height();

    bool onLeft = pos.x() <= bw;
    bool onRight = pos.x() >= w - bw;
    bool onTop = pos.y() <= bw;
    bool onBottom = pos.y() >= h - bw;

    if (onTop && onLeft) return ResizeEdge::TopLeft;
    if (onTop && onRight) return ResizeEdge::TopRight;
    if (onBottom && onLeft) return ResizeEdge::BottomLeft;
    if (onBottom && onRight) return ResizeEdge::BottomRight;
    if (onTop) return ResizeEdge::Top;
    if (onBottom) return ResizeEdge::Bottom;
    if (onLeft) return ResizeEdge::Left;
    if (onRight) return ResizeEdge::Right;

    return ResizeEdge::None;
}

void MainWindow::updateCursor(const QPoint &pos)
{
    ResizeEdge edge = getResizeEdge(pos);

    switch (edge) {
        case ResizeEdge::Top:
        case ResizeEdge::Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case ResizeEdge::Left:
        case ResizeEdge::Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case ResizeEdge::TopLeft:
        case ResizeEdge::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case ResizeEdge::TopRight:
        case ResizeEdge::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        resizeEdge = getResizeEdge(event->pos());
        if (resizeEdge != ResizeEdge::None)
        {
            resizeStartPos = event->globalPosition().toPoint();
            resizeStartGeometry = geometry();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (resizeEdge != ResizeEdge::None && (event->buttons() & Qt::LeftButton))
    {
        QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
        QRect newGeometry = resizeStartGeometry;

        switch (resizeEdge) {
            case ResizeEdge::Top:
                newGeometry.setTop(resizeStartGeometry.top() + delta.y());
                break;
            case ResizeEdge::Bottom:
                newGeometry.setBottom(resizeStartGeometry.bottom() + delta.y());
                break;
            case ResizeEdge::Left:
                newGeometry.setLeft(resizeStartGeometry.left() + delta.x());
                break;
            case ResizeEdge::Right:
                newGeometry.setRight(resizeStartGeometry.right() + delta.x());
                break;
            case ResizeEdge::TopLeft:
                newGeometry.setTopLeft(resizeStartGeometry.topLeft() + delta);
                break;
            case ResizeEdge::TopRight:
                newGeometry.setTop(resizeStartGeometry.top() + delta.y());
                newGeometry.setRight(resizeStartGeometry.right() + delta.x());
                break;
            case ResizeEdge::BottomLeft:
                newGeometry.setBottom(resizeStartGeometry.bottom() + delta.y());
                newGeometry.setLeft(resizeStartGeometry.left() + delta.x());
                break;
            case ResizeEdge::BottomRight:
                newGeometry.setBottomRight(resizeStartGeometry.bottomRight() + delta);
                break;
            default:
                break;
        }

        // Enforce minimum size
        if (newGeometry.width() >= minimumWidth() && newGeometry.height() >= minimumHeight())
        {
            setGeometry(newGeometry);
        }

        event->accept();
    }
    else
    {
        updateCursor(event->pos());
        QMainWindow::mouseMoveEvent(event);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        resizeEdge = ResizeEdge::None;
        event->accept();
    }
    QMainWindow::mouseReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == this && event->type() == QEvent::HoverMove)
    {
        QHoverEvent *hoverEvent = static_cast<QHoverEvent*>(event);
        updateCursor(hoverEvent->position().toPoint());
    }
    return QMainWindow::eventFilter(obj, event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG")
    {
        MSG* msg = static_cast<MSG*>(message);

        // Обработка горячих клавиш мультимедиа
        if (msg->message == WM_HOTKEY && bottomBar)
        {
            switch (static_cast<int>(msg->wParam))
            {
                case 1: // Play/Pause
                    bottomBar->playPause();
                    return true;
                case 2: // Next Track
                    bottomBar->playNext();
                    return true;
                case 3: // Previous Track
                    bottomBar->playPrevious();
                    return true;
                default:
                    break;
            }
        }

        // Блокируем Windows автоматическое изменение размера при DPI change
        if (msg->message == WM_DPICHANGED)
        {
            // Игнорируем предложенную Windows геометрию
            *result = 0;
            return true;
        }

        // Блокируем Windows Snap/Aero Snap
        if (msg->message == WM_NCCALCSIZE)
        {
            *result = 0;
            return true;
        }

        // Блокируем системное изменение размера
        if (msg->message == WM_WINDOWPOSCHANGING)
        {
            WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(msg->lParam);
            // Разрешаем только изменения позиции, но не размера от Windows
            if (resizeEdge == ResizeEdge::None && !isDragging())
            {
                pos->flags |= SWP_NOSIZE;
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::isDragging() const
{
    // Проверяем, идёт ли перетаскивание через TitleBar
    return false; // Упрощённая версия, можно улучшить
}
