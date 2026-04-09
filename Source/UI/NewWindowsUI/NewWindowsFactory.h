//
// Created for MusicPlayer
// NewWindows UI Factory - NO dependencies on Core in header!
//

#pragma once

#ifndef NEWWINDOWSFACTORY_H
#define NEWWINDOWSFACTORY_H

#include "UIFactory.h"

// Forward declaration only - NO includes!
class MainWindow;
class AppInstance;

class NewWindowsFactory : public IUIFactory {
public:
    NewWindowsFactory();
    ~NewWindowsFactory() override;

    // IUIFactory implementation
    QWidget* createMainWindow() override;

    const char* getName() const override { return "NewWindows"; }
    const char* getVersion() const override { return "2.0.0"; }
    const char* getPlatform() const override { return "windows"; }
    int getPriority() const override { return 100; }

    void initialize(AppInstance* app) override;
    void shutdown() override;

private:
    AppInstance* m_appInstance = nullptr;
};

#endif // NEWWINDOWSFACTORY_H
