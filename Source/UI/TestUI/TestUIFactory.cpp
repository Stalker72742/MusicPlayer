//
// TestUI Factory implementation
//

#include "TestUIFactory.h"
#include "TestMainWindow.h"  // Include ONLY in .cpp!
#include "AppInstance.h"
#include <QDebug>

TestUIFactory::TestUIFactory()
{
    qDebug() << "TestUIFactory created";
}

TestUIFactory::~TestUIFactory()
{
    qDebug() << "TestUIFactory destroyed";
}

QWidget* TestUIFactory::createMainWindow()
{
    qDebug() << "Creating TestUI MainWindow - Hello World!";
    return new TestMainWindow();
}

void TestUIFactory::initialize(AppInstance* app)
{
    m_appInstance = app;
    qDebug() << "TestUI initialized with AppInstance";
}

void TestUIFactory::shutdown()
{
    qDebug() << "TestUI shutting down";
    m_appInstance = nullptr;
}

// ============================================================================
// Export function for dynamic loading OR static registration
// ============================================================================

// For dynamic loading (.dll/.so)
UI_REGISTER_FACTORY(TestUIFactory, "TestUI")

// For static linking - auto-registers on startup with HIGHEST priority!
UI_AUTO_REGISTER(TestUIFactory, "TestUI")
