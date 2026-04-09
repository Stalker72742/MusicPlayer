//
// Created by Stalker7274 on 02.12.2025.
//

#include "TitleBar.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScreen>

TitleBar::TitleBar(QWidget *parent) : QWidget(parent)
{
    setupUI();
    setFixedHeight(40);

    // Инициализируем normalGeometry разумным размером по умолчанию
    normalGeometry = QRect(100, 100, 1200, 800);
}

TitleBar::~TitleBar()
{
}

void TitleBar::setupUI()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(0);

    QLineEdit* searchLine = new QLineEdit(this);

    searchLine->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    searchLine->setFixedHeight(23);

    layout->addSpacerItem(new QSpacerItem(120, 1));

    constexpr int32_t searchStretch = 30;
    constexpr int32_t sideStretch = (100 - searchStretch) / 2;

    layout->addStretch(sideStretch);
    layout->addWidget(searchLine, searchStretch, Qt::AlignVCenter);

    layout->addStretch(sideStretch);

    minimizeButton = new QPushButton("−", this);
    maximizeButton = new QPushButton("□", this);
    closeButton = new QPushButton("×", this);

    minimizeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    maximizeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    minimizeButton->setFixedSize(40, 40);
    maximizeButton->setFixedSize(40, 40);
    closeButton->setFixedSize(40, 40);

    // Styling
    QString buttonStyle =
        "QPushButton {"
        "   background-color: transparent;"
        "   color: white;"
        "   border: none;"
        "   font-size: 18px;"
        "   padding: 0px;"
        "   min-width: 40px;"
        "   max-width: 40px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 0.1);"
        "}";

    minimizeButton->setStyleSheet(buttonStyle);
    maximizeButton->setStyleSheet(buttonStyle);
    closeButton->setStyleSheet(buttonStyle +
        "QPushButton:hover {"
        "   background-color: rgba(255, 0, 0, 0.8);"
        "}");

    layout->addWidget(minimizeButton);
    layout->addWidget(maximizeButton);
    layout->addWidget(closeButton);

    connect(minimizeButton, &QPushButton::clicked, this, &TitleBar::onMinimizeClicked);
    connect(maximizeButton, &QPushButton::clicked, this, &TitleBar::onMaximizeClicked);
    connect(closeButton, &QPushButton::clicked, this, &TitleBar::onCloseClicked);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        isDragging = true;
        dragPosition = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton))
    {
        if (isMaximized)
        {
            const qreal ratio = static_cast<qreal>(event->position().x()) / width();

            isMaximized = false;
            maximizeButton->setText("□");

            QSize restoredSize = normalGeometry.size();

            int newX = event->globalPosition().toPoint().x() - static_cast<int>(restoredSize.width() * ratio);
            int newY = event->globalPosition().toPoint().y() - event->position().y();

            window()->setGeometry(newX, newY, restoredSize.width(), restoredSize.height());

            dragPosition.setX(static_cast<int>(restoredSize.width() * ratio));
            dragPosition.setY(event->position().y());

            return;
        }

        window()->move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        isDragging = false;
        event->accept();
    }
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        toggleMaximize();
        event->accept();
    }
}

void TitleBar::onMinimizeClicked()
{
    window()->showMinimized();
}

void TitleBar::onMaximizeClicked()
{
    toggleMaximize();
}

void TitleBar::onCloseClicked()
{
    window()->close();
}

void TitleBar::toggleMaximize()
{
    if (!isMaximized)
    {
        QRect currentGeometry = window()->geometry();
        QScreen *currentScreen = QApplication::screenAt(currentGeometry.center());
        if (!currentScreen)
            currentScreen = QApplication::primaryScreen();

        QRect screenGeometry = currentScreen->availableGeometry();

        if (currentGeometry.size() != screenGeometry.size())
        {
            normalGeometry = currentGeometry;
        }

        window()->setGeometry(screenGeometry);

        isMaximized = true;
        maximizeButton->setText("❐");
    }
    else
    {
        window()->setGeometry(normalGeometry);

        isMaximized = false;
        maximizeButton->setText("□");
    }
}
