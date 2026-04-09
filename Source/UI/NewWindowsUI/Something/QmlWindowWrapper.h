#pragma once
#include <QWidget>

class QQmlApplicationEngine;
class AppInstance;

// Обёртка вокруг QML engine — UIPluginSystem хочет QWidget*, даём ему это
// Engine создаётся здесь, не в main.cpp
class QmlWindowWrapper : public QWidget
{
    Q_OBJECT
public:
    explicit QmlWindowWrapper(QWidget* parent = nullptr);
    ~QmlWindowWrapper() override;

    // Прокинуть что-нибудь в QML контекст до загрузки
    void setContextProperty(const QString& name, QObject* obj);

private:
    void setupEngine();

    QQmlApplicationEngine* m_engine = nullptr;
};