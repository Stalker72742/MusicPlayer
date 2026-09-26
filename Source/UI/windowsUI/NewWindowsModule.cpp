#include "NewWindowsModule.h"

#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>
#include <QQuickWindow>
#include "eventDispatcher/EventDispatcher.h"
#include "Models/MedialibModel.h"
#include "modSubsystem/PluginFactory.h"

PLUGIN_EXPORT(NewWindowsModule);

void NewWindowsModule::messageReceived(const QString &message)
{
	QQuickWindow* window = nullptr;
	if (!m_engine || m_engine->rootObjects().isEmpty() || !m_engine->rootObjects()[0])
	{
		return;
	}

	window = qobject_cast<QQuickWindow*>(m_engine->rootObjects()[0]);

	if (!window)
	{
		return;
	}

    if (message == "ShowWindow")
    {
    	window->show();

    } else if (message == "HideWindow")
    {
    	window->hide();

    } else if (message == "InvertVisibility")
    {
	    if (window->isVisible())
	    {
		    window->hide();
	    }else
	    {
		    window->show();
	    }
    }
}

void NewWindowsModule::init() {

    m_engine = new QQmlApplicationEngine();

    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Widgets/Theme.qml")),
                             "SoundLink", 1, 0, "Theme");

	qmlRegisterType<MedialibModel>("SoundLink", 1, 0, "MedialibModel");

    m_engine->load(QUrl(QStringLiteral("qrc:/Widgets/WindowsMainWindow.qml")));

    if (m_engine->rootObjects().isEmpty())
        qCritical() << "[NewWindows] QML load FAILED — проверь qrc и путь";
    else
        qDebug() << "[NewWindows] qml loaded";

    if (auto eventDispatcher = SubsystemBase::GetSubsystem<eventDisp>()) {

        QObject::connect(eventDispatcher, &eventDisp::messageReceived, eventDispatcher, [this](const QString& message) {
            messageReceived(message);
        });
    }
}

void NewWindowsModule::shutdown() {

    if (m_engine) {

        m_engine->deleteLater();
        m_engine = nullptr;
    }
}
