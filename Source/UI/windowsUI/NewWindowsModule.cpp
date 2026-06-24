#include "NewWindowsModule.h"

#include <QDirIterator>
#include <qguiapplication.h>
#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>

void NewWindowsModule::init() {

    m_engine = new QQmlApplicationEngine();

    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Widgets/Theme.qml")),
                             "SoundLink", 1, 0, "Theme");

    m_engine->load(QUrl(QStringLiteral("qrc:/Widgets/WindowsMainWindow.qml")));

    if (m_engine->rootObjects().isEmpty())
        qCritical() << "[NewWindows] QML load FAILED — проверь qrc и путь";
    else
        qDebug() << "[NewWindows] QML OK";
}

void NewWindowsModule::shutdown() {

    if (m_engine) {

        m_engine->deleteLater();
        m_engine = nullptr;
    }
}
