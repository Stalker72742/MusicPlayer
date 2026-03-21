#include "NewWindowsModule.h"
#include "AppInstance.h"

#include <QMainWindow>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include "Something/QmlWindowWrapper.h"
#include "UIPluginSystem.h"

void NewWindowsModule::StartupModule()
{
    // Здесь вся инициализация модуля:
    // субсистемы, сервисы, и создание UI — всё тут
    AppInstance* app = AppInstance::getInstance();

    // Пример: зарегать субсистему специфичную для этого UI
    // app->addSubsystem(new SomeSubsystem());

    QQuickStyle::setStyle("Fusion");

    // engine — child этого объекта, удалится сам
    auto m_engine = new QQmlApplicationEngine();

    // Прокидываем AppInstance в QML если нужно
    // m_engine->rootContext()->setContextProperty("app", AppInstance::getInstance());

    m_engine->load(QUrl(QStringLiteral("qrc:/Widgets/WindowsMainWindow.qml")));

    if (m_engine->rootObjects().isEmpty())
        qCritical() << "[NewWindows] QML load FAILED — проверь qrc и путь";
    else
        qDebug() << "[NewWindows] QML OK";

    qDebug() << "[NewWindowsModule] StartupModule";
}

void NewWindowsModule::ShutdownModule()
{
    if (m_window)
    {
        m_window->close();
        m_window->deleteLater();
        m_window = nullptr;
    }

    qDebug() << "[NewWindowsModule] ShutdownModule";
}