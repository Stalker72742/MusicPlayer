#include "TrayModule.h"

#include <QApplication>
#include <qcoreapplication.h>
#include <QDirIterator>
#include <QSystemTrayIcon>
#include <QMenu>
#include "eventDispatcher/EventDispatcher.h"
#include "modSubsystem/PluginFactory.h"

PLUGIN_EXPORT(TrayModule);

void TrayModule::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::ActivationReason::Trigger) {
        invertVisibility();
    }
}

void TrayModule::showActionTriggered(bool checked)
{
    showWindow();
}

void TrayModule::hideActionTriggered(bool checked)
{
	hideWindow();
}

void TrayModule::quitActionTriggered(bool checked)
{
	QApplication::quit();
}

void TrayModule::showWindow()
{
    if (auto eventDispatcher = SubsystemBase::GetSubsystem<eventDisp>()) {

        eventDispatcher->sendMessage("ShowWindow");
    }
}

void TrayModule::hideWindow()
{
	if (auto eventDispatcher = SubsystemBase::GetSubsystem<eventDisp>()) {

		eventDispatcher->sendMessage("HideWindow");
	}
}

void TrayModule::invertVisibility()
{
	if (auto eventDispatcher = SubsystemBase::GetSubsystem<eventDisp>()) {

		eventDispatcher->sendMessage("InvertVisibility");
	}
}

void TrayModule::init()
{
    trayIcon = new QSystemTrayIcon();

    QIcon icon(":/icons/ApplicationIcon/icon_512.png");
    if (icon.isNull())
        return;

    trayIcon->setIcon(icon);
    trayIcon->setToolTip("SoundLink");

    trayMenu = new QMenu();
    showAction = new QAction("Show", trayMenu);
	hideAction = new QAction("Hide", trayMenu);
	quitAction = new QAction("Quit", trayMenu);

    trayMenu->addAction(showAction);
	trayMenu->addAction(hideAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);

	trayMenu->setStyleSheet(R"(
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

    trayIcon->setContextMenu(trayMenu);

    trayIcon->show();

    connect(showAction, &QAction::triggered, this, &TrayModule::showActionTriggered);
	connect(hideAction, &QAction::triggered, this, &TrayModule::hideActionTriggered);
	connect(quitAction, &QAction::triggered, this, &TrayModule::quitActionTriggered);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &TrayModule::onTrayActivated);
}

void TrayModule::shutdown()
{
	if (trayIcon)
		trayIcon->deleteLater();

	if (trayMenu)
		trayMenu->deleteLater();
}
