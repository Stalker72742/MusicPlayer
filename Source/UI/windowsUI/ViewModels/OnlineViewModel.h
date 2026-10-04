#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// @brief Online backends and the browser extension for the UI (Settings › Online, the results screen). QML singleton.
class OnlineViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(BridgeState bridgeState READ GetBridgeState NOTIFY bridgeChanged)
    Q_PROPERTY(QString browserName READ GetBrowserName NOTIFY bridgeChanged)
    Q_PROPERTY(QString pairingCode READ GetPairingCode NOTIFY bridgeChanged)
    Q_PROPERTY(int pairedCount READ GetPairedCount NOTIFY bridgeChanged)

    /// @brief [{ name, roles ("search", "streams" or both), status ("" when usable) }]
    Q_PROPERTY(QVariantList backends READ GetBackends NOTIFY backendsChanged)
    Q_PROPERTY(QString lastSearchBackend READ GetLastSearchBackend NOTIFY backendsChanged)
    Q_PROPERTY(QString lastStreamBackend READ GetLastStreamBackend NOTIFY backendsChanged)

public:
    /// @brief Mirrors BrowserBridge::State.
    enum BridgeState
    {
        Stopped,
        Waiting,
        PairingRequired,
        Connected
    };
    Q_ENUM(BridgeState)

    /// @brief The instance shared by C++ and QML.
    static OnlineViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static OnlineViewModel* create(QQmlEngine*, QJSEngine*);

    BridgeState GetBridgeState() const;
    QString GetBrowserName() const;
    QString GetPairingCode() const;
    int GetPairedCount() const;

    QVariantList GetBackends() const;
    QString GetLastSearchBackend() const;
    QString GetLastStreamBackend() const;

    /// @brief See BrowserBridge::ForgetPairedBrowsers().
    Q_INVOKABLE void forgetPairedBrowsers();

    /// @brief Unpacks the bundled extension to `<AppLocalData>/browser-extension` and opens the folder,
    /// for "Load unpacked" (Chrome) or "Load Temporary Add-on" (Firefox).
    Q_INVOKABLE void openExtensionFolder();

signals:
    /// @brief BrowserBridge state changed.
    void bridgeChanged();
    /// @brief Backend statuses or the last used backends changed.
    void backendsChanged();

private:
    OnlineViewModel();
};
