//
// Created by Stalker7274 on 17.04.2025.
//

#include "AppInstance.h"

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include <QAppUpdater/AppControlServer.h>

#include "AppConfigs.h"
#include "Discord/DiscordSubsystem.h"
#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "Online/BrowserBridge.h"
#include "SystemMediaControls.h"
#include "TrayIcon.h"
#include "YtDlp/DenoSubsystem.h"
#include "YtDlp/FfmpegSubsystem.h"
#include "YtDlp/YtDlpSubsystem.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

AppInstance* AppInstance::instance = nullptr;

AppInstance::AppInstance()
{
    Q_ASSERT_X(!instance, "AppInstance", "only one instance is allowed");
    instance = this;
}

AppInstance::~AppInstance()
{
    delete mediaControls;
    mediaControls = nullptr;

    delete engine;
    engine = nullptr;

    delete trayIcon;
    trayIcon = nullptr;

    instance = nullptr;
}

AppInstance* AppInstance::getInstance()
{
    return instance;
}

bool AppInstance::init()
{
    QIcon appIcon(QStringLiteral(":/icons/ApplicationIcon/tray_16.png"));
    for (int size : {20, 24, 32, 40, 48})
        appIcon.addFile(QStringLiteral(":/icons/ApplicationIcon/taskbar_%1.png").arg(size), QSize(size, size));
    appIcon.addFile(QStringLiteral(":/icons/ApplicationIcon/icon_512.png"), QSize(512, 512));
    QApplication::setWindowIcon(appIcon);

    QApplication::setQuitOnLastWindowClosed(false);

    AppConfigs::RegisterAppConfigs();

    LibrarySubsystem::Get().Initialize();
    DownloadSubsystem::Get().Initialize();

    controlServer = new QAppUpdater::AppControlServer(this);
    connect(controlServer, &QAppUpdater::AppControlServer::quitRequested, qApp, &QCoreApplication::quit,
        Qt::QueuedConnection);

    if (!createMainWindow())
        return false;

    createTrayIcon();
    createMediaControls();

    DiscordSubsystem::Get().Start();

    if (AppConfigs::GetBool(AppConfigs::SettingsKeys::StartMinimized))
        hideMainWindow();

    YtDlpSubsystem::Get().StartAutoUpdate();
    FfmpegSubsystem::Get().EnsureInstalled();
    DenoSubsystem::Get().EnsureInstalled();

    BrowserBridge::Get().Start();
    return true;
}

QQuickWindow* AppInstance::getMainWindow() const
{
    if (!engine || engine->rootObjects().isEmpty())
        return nullptr;

    return qobject_cast<QQuickWindow*>(engine->rootObjects().first());
}

void AppInstance::showMainWindow()
{
    if (QQuickWindow* window = getMainWindow())
    {
        window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
        window->show();
        window->raise();
        window->requestActivate();
    }
}

void AppInstance::hideMainWindow()
{
    if (QQuickWindow* window = getMainWindow())
        window->hide();
}

void AppInstance::toggleMainWindow()
{
    QQuickWindow* window = getMainWindow();
    if (!window)
        return;

    if (window->isVisible() && !(window->windowStates() & Qt::WindowMinimized))
        hideMainWindow();
    else
        showMainWindow();
}

bool AppInstance::createMainWindow()
{
    engine = new QQmlApplicationEngine();
    engine->loadFromModule("SoundLink", "WindowsMainWindow");

    if (!getMainWindow())
    {
        qCritical() << "[AppInstance] Failed to load the main window";
        return false;
    }

    allowTaskbarMinimize();
    connect(getMainWindow(), &QWindow::visibleChanged, this, &AppInstance::allowTaskbarMinimize);
    return true;
}

void AppInstance::allowTaskbarMinimize()
{
#ifdef Q_OS_WIN
    QQuickWindow* window = getMainWindow();
    if (!window)
        return;

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (!(style & WS_MINIMIZEBOX))
        SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_MINIMIZEBOX);
#endif
}

void AppInstance::createMediaControls()
{
    mediaControls = new SystemMediaControls(getMainWindow());
}

void AppInstance::createTrayIcon()
{
    trayIcon = new TrayIcon();

    connect(trayIcon, &TrayIcon::showRequested, this, &AppInstance::showMainWindow);
    connect(trayIcon, &TrayIcon::hideRequested, this, &AppInstance::hideMainWindow);
    connect(trayIcon, &TrayIcon::toggleRequested, this, &AppInstance::toggleMainWindow);
    connect(trayIcon, &TrayIcon::quitRequested, qApp, &QCoreApplication::quit);

    trayIcon->show();
}
