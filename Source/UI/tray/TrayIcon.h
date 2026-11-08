#pragma once

#include <QObject>
#include <QSystemTrayIcon>

#include <functional>
#include <memory>

class QAction;
class QMenu;

/// @brief Tray icon with a menu: one item that shows or hides the window, the items of modules, Exit.
///
/// Only emits requests; AppInstance connects them to its slots and tells it whether the window is shown.
/// Clicking the icon requests a toggle too.
///
/// The menu is a dark QMenu that the icon opens itself: given to QSystemTrayIcon, Qt would turn it into a native
/// Win32 menu in an app with a Qt Quick window, and that ignores the style sheet.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    /// @brief Shows the icon in the tray.
    void show();

    /// @brief Shows a short notification next to the tray.
    /// @param onClick Called if the user clicks this notification.
    void ShowMessage(const QString& title, const QString& text, std::function<void()> onClick = {});

    /// @brief Makes the window item say "Hide SoundLink" or "Show SoundLink".
    void SetWindowShown(bool bShown);

    /// @brief Adds an item above Exit; for modules (see AppModule).
    QAction* AddAction(const QString& text, std::function<void()> onTriggered);

signals:
    /// @brief The window item was chosen or the icon was clicked.
    void toggleRequested();
    /// @brief "Exit" was chosen in the menu.
    void quitRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void OpenMenu();

    QSystemTrayIcon* trayIcon {nullptr};

    /// QMenu is a widget and cannot be a QObject child of the tray icon.
    std::unique_ptr<QMenu> menu;
    QAction* windowAction {nullptr};
    /// Module items go above this separator.
    QAction* exitSeparator {nullptr};
    std::function<void()> messageClickHandler;
};
