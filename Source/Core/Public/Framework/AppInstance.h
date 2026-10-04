//
// Created by Stalker7274 on 17.04.2025.
//

#pragma once

#include <QObject>

class QQmlApplicationEngine;
namespace QAppUpdater { class AppControlServer; }
class QQuickWindow;
class SystemMediaControls;
class TrayIcon;

/// @brief Owns the UI: the QML engine with the main window, the tray icon and the OS media controls.
///
/// Subsystems are independent singletons and are not owned here. Create one instance on the stack
/// in main() after QApplication.
class AppInstance : public QObject
{
    Q_OBJECT

public:
    AppInstance();
    ~AppInstance() override;

    AppInstance(const AppInstance&) = delete;
    AppInstance& operator=(const AppInstance&) = delete;

    /// @brief The running instance; nullptr before construction and after destruction.
    static AppInstance* getInstance();

    /// @brief Registers configs, starts the subsystems and loads the main window.
    /// @return false if the main window failed to load.
    bool init();

    /// @brief The QML engine that holds the main window.
    QQmlApplicationEngine* getEngine() const { return engine; }

    /// @brief The updater's line to this app: quit requests and, during a windowless install, its progress.
    QAppUpdater::AppControlServer* getControlServer() const { return controlServer; }

    /// @brief The root window of the QML engine, or nullptr if it is not loaded.
    QQuickWindow* getMainWindow() const;

public slots:
    /// @brief Shows, restores and activates the main window.
    void showMainWindow();
    /// @brief Hides the main window to the tray.
    void hideMainWindow();
    /// @brief Shows the main window if it is hidden, hides it otherwise.
    void toggleMainWindow();

private:
    bool createMainWindow();
    void createTrayIcon();
    void createMediaControls();

    QQmlApplicationEngine* engine = nullptr;
    TrayIcon* trayIcon = nullptr;
    SystemMediaControls* mediaControls = nullptr;
    QAppUpdater::AppControlServer* controlServer = nullptr;

    static AppInstance* instance;
};
