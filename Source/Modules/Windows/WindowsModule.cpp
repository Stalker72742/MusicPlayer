#include "WindowsModule.h"
#include "IModuleInterface.h"
#include "AppInstance.h"
#include "PlayerSubsystem.h"
#include "windowsmediaplayer.h"
#include <QDebug>

void WindowsModule::StartupModule()
{
    AppInstance* app = AppInstance::getInstance();

    // Регистрируем субсистемы
    //app->addSubsystem(new PlayerSubsystem(new WindowsMediaPlayer(nullptr)));

    // Инициализируем UI Plugin System (сканирует .dll, грузит конфиг)
    // UI-фабрики зарегистрируются сами через UI_AUTO_REGISTER / scanForPlugins
    app->initializeUISystem();

    QQuickStyle::setStyle("Fusion");

    m_engine = new QQmlApplicationEngine(this);

    // Можно прокинуть что угодно в контекст
    // m_engine->rootContext()->setContextProperty("myModel", someModel);

    m_engine->load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));

    if (m_engine->rootObjects().isEmpty())
        qCritical() << "[QmlWindowWrapper] Failed to load QML!";
    else
        qDebug() << "[QmlWindowWrapper] QML loaded OK";

    qDebug() << "[WindowsModule] StartupModule";
}

void WindowsModule::ShutdownModule()
{
    qDebug() << "[WindowsModule] ShutdownModule";
}
