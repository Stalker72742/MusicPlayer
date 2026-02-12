//
// TestUI Factory - Zero dependency factory
//

#pragma once

#ifndef TESTUI_FACTORY_H
#define TESTUI_FACTORY_H

#include "UIFactory.h"

// Forward declarations only - NO includes!
class TestMainWindow;
class AppInstance;

class TestUIFactory : public IUIFactory {
public:
    TestUIFactory();
    ~TestUIFactory() override;

    // IUIFactory implementation
    QWidget* createMainWindow() override;

    const char* getName() const override { return "TestUI"; }
    const char* getVersion() const override { return "1.0.0"; }
    const char* getPlatform() const override { return "windows"; }
    int getPriority() const override { return 200; }  // Highest priority for testing!

    void initialize(AppInstance* app) override;
    void shutdown() override;

private:
    AppInstance* m_appInstance = nullptr;
};

#endif // TESTUI_FACTORY_H
