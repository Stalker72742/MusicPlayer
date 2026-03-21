#include "QmlWindowWrapper.h"
#include "AppInstance.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>
#include <QDebug>

QmlWindowWrapper::QmlWindowWrapper(QWidget* parent)
    : QWidget(parent)
{
    setupEngine();
}

QmlWindowWrapper::~QmlWindowWrapper() = default;

void QmlWindowWrapper::setContextProperty(const QString& name, QObject* obj)
{
    if (m_engine)
        m_engine->rootContext()->setContextProperty(name, obj);
}

void QmlWindowWrapper::setupEngine()
{
    QQuickStyle::setStyle("Fusion");

    // engine — child этого объекта, удалится сам
    m_engine = new QQmlApplicationEngine(this);

    // Прокидываем AppInstance в QML если нужно
    // m_engine->rootContext()->setContextProperty("app", AppInstance::getInstance());

    m_engine->load(QUrl(QStringLiteral("qrc:/Widgets/WindowsMainWindow.qml")));

    if (m_engine->rootObjects().isEmpty())
        qCritical() << "[NewWindows] QML load FAILED — проверь qrc и путь";
    else
        qDebug() << "[NewWindows] QML OK";

    // Сам QWidget прячем — QML создаёт своё нативное окно через ApplicationWindow
    hide();
}