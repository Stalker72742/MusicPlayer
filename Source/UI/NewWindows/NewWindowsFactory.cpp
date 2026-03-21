#include "NewWindowsFactory.h"
#include "Something/QmlWindowWrapper.h"   // <-- теперь здесь, не MainWindow
#include "AppInstance.h"
#include <QDebug>

NewWindowsFactory::NewWindowsFactory()
{
    qDebug() << "[NewWindowsFactory] created";
}

NewWindowsFactory::~NewWindowsFactory()
{
    qDebug() << "[NewWindowsFactory] destroyed";
}

QWidget* NewWindowsFactory::createMainWindow()
{
    qDebug() << "[NewWindowsFactory] creating QML window";
    auto* wrapper = new QmlWindowWrapper();

    // Хочешь передать модели в QML — делай здесь:
    // wrapper->setContextProperty("playerData", AppInstance::getInstance()->getSubsystem<PlayerSubsystem>());

    return wrapper;
}

void NewWindowsFactory::initialize(AppInstance* app)
{
    m_appInstance = app;
    qDebug() << "[NewWindowsFactory] initialized";
}

void NewWindowsFactory::shutdown()
{
    m_appInstance = nullptr;
    qDebug() << "[NewWindowsFactory] shutdown";
}

UI_REGISTER_FACTORY(NewWindowsFactory, "NewWindows")
UI_AUTO_REGISTER(NewWindowsFactory, "NewWindows")