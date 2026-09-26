
#pragma once

#include <qaction.h>
#include <QSystemTrayIcon>

#include "IModuleInterface.h"

class TrayModule : public QObject, public IPlugin
{
    Q_OBJECT
protected:

	QSystemTrayIcon* trayIcon {nullptr};
	QMenu* trayMenu {nullptr};
	QAction* showAction {nullptr};
	QAction* hideAction {nullptr};
	QAction* quitAction {nullptr};

protected slots:

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void showActionTriggered(bool checked);
	void hideActionTriggered(bool checked);
	void quitActionTriggered(bool checked);

protected:

    void showWindow();
    void hideWindow();
    void invertVisibility();

public:
    void init() override;
    void shutdown() override;
    QString name() const override { return QStringLiteral("Tray"); }
};