#include "NewWindowsModule.h"

#include <QDirIterator>
#include <QFile>
#include <qguiapplication.h>
#include <QQmlApplicationEngine>
#include "Something/QmlWindowWrapper.h"
#include "UIPluginSystem.h"

void NewWindowsModule::StartupModule()
{

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

void NewWindowsModule::init() {
    qDebug() << "[NewWindowsModul1e] Init";

    auto m_engine = new QQmlApplicationEngine();

    // Прокидываем AppInstance в QML если нужно
    // m_engine->rootContext()->setContextProperty("app", AppInstance::getInstance());

    m_engine->load(QUrl(QStringLiteral("qrc:/Widgets/WindowsMainWindow.qml")));

    if (m_engine->rootObjects().isEmpty())
        qCritical() << "[NewWindows] QML load FAILED — проверь qrc и путь";
    else
        qDebug() << "[NewWindows] QML OK";

    QGuiApplication::setWindowIcon(QIcon("/icons/ApplicationIcon/AppIcon.ico"));

    qDebug() << "Find icon result: " << QFile::exists("/icons/ApplicationIcon/AppIcon.ico");

    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext())
        qDebug() << it.next();
}

void NewWindowsModule::shutdown() {
    qDebug() << "[NewWindowsModule] ShutdownModule";

    if (m_window)
    {
        m_window->close();
        m_window->deleteLater();
        m_window = nullptr;
    }
}
