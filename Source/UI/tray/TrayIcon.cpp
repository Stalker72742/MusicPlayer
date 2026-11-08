#include "TrayIcon.h"

#include <QAction>
#include <QCursor>
#include <QIcon>
#include <QMenu>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

    windowAction = menu->addAction(QStringLiteral("Hide SoundLink"), this, &TrayIcon::toggleRequested);
    exitSeparator = menu->addSeparator();
    menu->addAction(QStringLiteral("Exit"), this, &TrayIcon::quitRequested);

    menu->setStyleSheet(R"(
        QMenu {
            background-color: #151515;
            color: #fff;
            border: 1px solid #2a2a2a;
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

    connect(trayIcon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    connect(trayIcon, &QSystemTrayIcon::messageClicked, this, [this] {
        if (messageClickHandler)
            messageClickHandler();
    });
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::show()
{
    trayIcon->show();
}

void TrayIcon::ShowMessage(const QString& title, const QString& text, std::function<void()> onClick)
{
    messageClickHandler = std::move(onClick);
    trayIcon->showMessage(title, text);
}

void TrayIcon::SetWindowShown(bool bShown)
{
    windowAction->setText(bShown ? QStringLiteral("Hide SoundLink") : QStringLiteral("Show SoundLink"));
}

QAction* TrayIcon::AddAction(const QString& text, std::function<void()> onTriggered)
{
    auto* action = new QAction(text, menu.get());
    connect(action, &QAction::triggered, this, [onTriggered = std::move(onTriggered)] { onTriggered(); });

    const QList<QAction*> actions = menu->actions();
    const bool bFirstModuleItem = actions.indexOf(exitSeparator) == actions.indexOf(windowAction) + 1;
    if (bFirstModuleItem)
        menu->insertSeparator(exitSeparator);
    menu->insertAction(exitSeparator, action);
    return action;
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger)
        emit toggleRequested();
    else if (reason == QSystemTrayIcon::Context)
        OpenMenu();
}

void TrayIcon::OpenMenu()
{
    menu->popup(QCursor::pos());
#ifdef Q_OS_WIN
    SetForegroundWindow(reinterpret_cast<HWND>(menu->winId()));
#endif
    menu->activateWindow();
}
