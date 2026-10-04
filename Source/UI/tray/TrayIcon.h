#pragma once

#include <QObject>
#include <QSystemTrayIcon>

#include <memory>

class QMenu;

/// @brief Tray icon with a Show / Hide / Quit menu.
///
/// Only emits requests; AppInstance connects them to its slots. Clicking the icon requests a toggle.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    /// @brief Shows the icon in the tray.
    void show();

signals:
    /// @brief "Show" was chosen in the menu.
    void showRequested();
    /// @brief "Hide" was chosen in the menu.
    void hideRequested();
    /// @brief The icon was clicked.
    void toggleRequested();
    /// @brief "Quit" was chosen in the menu.
    void quitRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QSystemTrayIcon* trayIcon {nullptr};

    /// QMenu is a widget and cannot be a QObject child of the tray icon.
    std::unique_ptr<QMenu> menu;
};
