#include "TrayIcon.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
    , trayIcon(new QSystemTrayIcon(this))
    , menu(std::make_unique<QMenu>())
{
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48})
        icon.addFile(QStringLiteral(":/icons/ApplicationIcon/tray_%1.png").arg(size), QSize(size, size));
    trayIcon->setIcon(icon);
    trayIcon->setToolTip(QStringLiteral("SoundLink"));

    menu->addAction(QStringLiteral("Show"), this, &TrayIcon::showRequested);
    menu->addAction(QStringLiteral("Hide"), this, &TrayIcon::hideRequested);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Quit"), this, &TrayIcon::quitRequested);

    menu->setStyleSheet(R"(
        QMenu {
            background-color: #151515;
            color: #fff;
            border: none;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            padding: 6px 24px 6px 12px;
        }
        QMenu::item:selected {
            background-color: #3a3a3a;
        }
        QMenu::separator {
            height: 1px;
            background: #454545;
            margin: 4px 8px;
        }
    )");

    trayIcon->setContextMenu(menu.get());

    connect(trayIcon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::show()
{
    trayIcon->show();
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger)
        emit toggleRequested();
}
