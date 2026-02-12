//
// Created for MusicPlayer
// OldWindows UI Factory implementation
//

#include "OldWindowsFactory.h"
#include "mainwindow.h"  // Include ONLY in .cpp!
#include "AppInstance.h"
#include <QDebug>

OldWindowsFactory::OldWindowsFactory()
{
    qDebug() << "OldWindowsFactory created";
}

OldWindowsFactory::~OldWindowsFactory()
{
    qDebug() << "OldWindowsFactory destroyed";
}

QWidget* OldWindowsFactory::createMainWindow()
{
    qDebug() << "Creating OldWindows mainWindow";
    return new mainWindow();
}

void OldWindowsFactory::initialize(AppInstance* app)
{
    m_appInstance = app;
    qDebug() << "OldWindows UI initialized with AppInstance";
}

void OldWindowsFactory::shutdown()
{
    qDebug() << "OldWindows UI shutting down";
    m_appInstance = nullptr;
}

// ============================================================================
// Export function for dynamic loading OR static registration
// ============================================================================

// For dynamic loading (.dll/.so)
UI_REGISTER_FACTORY(OldWindowsFactory, "OldWindows")

// For static linking - auto-registers on startup
UI_AUTO_REGISTER(OldWindowsFactory, "OldWindows")
