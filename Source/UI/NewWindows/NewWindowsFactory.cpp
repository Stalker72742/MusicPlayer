//
// Created for MusicPlayer
// NewWindows UI Factory implementation
//

#include "NewWindowsFactory.h"
#include "MainWindow.h"  // Include ONLY in .cpp!
#include "AppInstance.h"
#include <QDebug>

NewWindowsFactory::NewWindowsFactory()
{
    qDebug() << "NewWindowsFactory created";
}

NewWindowsFactory::~NewWindowsFactory()
{
    qDebug() << "NewWindowsFactory destroyed";
}

QWidget* NewWindowsFactory::createMainWindow()
{
    qDebug() << "Creating NewWindows MainWindow";
    return new MainWindow();
}

void NewWindowsFactory::initialize(AppInstance* app)
{
    m_appInstance = app;
    qDebug() << "NewWindows UI initialized with AppInstance";
}

void NewWindowsFactory::shutdown()
{
    qDebug() << "NewWindows UI shutting down";
    m_appInstance = nullptr;
}

// ============================================================================
// Export function for dynamic loading OR static registration
// ============================================================================

// For dynamic loading (.dll/.so)
UI_REGISTER_FACTORY(NewWindowsFactory, "NewWindows")

// For static linking - auto-registers on startup
UI_AUTO_REGISTER(NewWindowsFactory, "NewWindows")
