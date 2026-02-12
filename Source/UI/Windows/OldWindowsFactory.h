//
// Created for MusicPlayer
// OldWindows UI Factory - NO dependencies on Core in header!
//

#pragma once

#ifndef OLDWINDOWSFACTORY_H
#define OLDWINDOWSFACTORY_H

#include "UIFactory.h"

// Forward declaration only - NO includes!
class mainWindow;
class AppInstance;

class OldWindowsFactory : public IUIFactory {
public:
    OldWindowsFactory();
    ~OldWindowsFactory() override;

    // IUIFactory implementation
    QWidget* createMainWindow() override;

    const char* getName() const override { return "OldWindows"; }
    const char* getVersion() const override { return "1.0.0"; }
    const char* getPlatform() const override { return "windows"; }
    int getPriority() const override { return 50; }

    void initialize(AppInstance* app) override;
    void shutdown() override;

private:
    AppInstance* m_appInstance = nullptr;
};

#endif // OLDWINDOWSFACTORY_H
