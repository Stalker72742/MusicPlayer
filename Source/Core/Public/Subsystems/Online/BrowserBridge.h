//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QString>

#include <functional>

class QTimer;
class QWebSocket;
class QWebSocketServer;

/// @brief Connection to the SoundLink browser extension (`MusicPlayer/extension`).
///
/// The app listens on 127.0.0.1, the extension keeps trying to connect; neither needs the other, a missing
/// extension just means the "browser" backend is unavailable.
///
/// Security: only loopback; only browser extension origins (web pages cannot fake Origin); and every
/// extension must be paired once - the app shows a 6-digit code, the user types it into the extension,
/// which then gets a random token. Only SHA-256 hashes of tokens are stored (`<AppLocalData>/browser-bridge.json`).
/// Nothing is requested from an unpaired connection.
///
/// Protocol: JSON text frames.
/// - Extension: `hello {browser, token?}`, `pair {code}`, `ping`, `response {id, result|error}`.
/// - App: `welcome {paired}`, `paired {token}`, `pairFailed {attemptsLeft}`, `pong`, `request {id, method, params}`.
class BrowserBridge : public Subsystem<BrowserBridge>
{
    Q_OBJECT
    friend class Subsystem<BrowserBridge>;

public:
    /// @brief Connection state shown in the UI.
    enum class State
    {
        Stopped,         ///< Could not listen.
        Waiting,         ///< No extension connected.
        PairingRequired, ///< An unpaired extension waits for the code.
        Connected        ///< A paired extension is connected.
    };

    /// @brief Starts listening on the first free port of a small fixed range.
    ///
    /// The extension tries the same ports; keep the list in sync with `extension/background.js`.
    void Start();

    State GetState() const { return state; }
    bool IsConnected() const { return state == State::Connected; }
    /// @brief Browser name reported by a connected paired extension; empty if there is none.
    QString GetBrowserName() const;
    /// @brief The port being listened on, 0 if stopped.
    int GetPort() const { return port; }

    /// @brief The 6-digit pairing code, shown while State::PairingRequired.
    QString GetPairingCode() const { return pairingCode; }

    /// @brief Number of paired extensions.
    int GetPairedCount() const { return static_cast<int>(pairedHashes.size()); }

    /// @brief Revokes every paired extension and drops their connections.
    void ForgetPairedBrowsers();

    /// @brief Result of Request().
    ///
    /// `result` is empty on error; `error` says why ("not connected", timeout, the extension's own error).
    using ResponseCallback = std::function<void(const QJsonObject& result, const QString& error)>;

    /// @brief Sends a request to the most recently active paired extension.
    /// @param method `"credentials"` or `"cookies"`.
    /// @param params Method parameters.
    /// @param context The callback is dropped if this object dies first.
    /// @param callback Called once, on the main thread.
    /// @param timeoutMs Time to wait for the response.
    void Request(const QString& method, const QJsonObject& params, QObject* context, ResponseCallback callback,
        int timeoutMs = 10'000);

signals:
    /// @brief Emitted whenever connections, pairing or the pairing code change.
    void stateChanged();

private:
    BrowserBridge();

    void Deinitialize() override;

    struct Client
    {
        QWebSocket* socket = nullptr;
        QTimer* deadline = nullptr;
        QString browser;
        bool bHello = false;
        bool bPaired = false;
        int pairAttempts = 0;
        QDateTime lastSeen;
    };

    struct Pending
    {
        QPointer<QObject> context;
        ResponseCallback callback;
        QTimer* timer = nullptr;
        QWebSocket* socket = nullptr;
    };

    void OnNewConnection();
    void OnMessage(QWebSocket* socket, const QString& message);
    void OnDisconnected(QWebSocket* socket);

    void HandleHello(Client& client, const QJsonObject& message);
    void HandlePair(Client& client, const QJsonObject& message);
    void HandleResponse(const QJsonObject& message);

    void Send(QWebSocket* socket, const QJsonObject& message);
    void Drop(QWebSocket* socket, const QString& reason);
    void FailPending(QWebSocket* socket, const QString& error);

    Client* FindClient(QWebSocket* socket);
    Client* ActiveClient();

    void EnsurePairingCode();
    void UpdateState();

    static QByteArray HashToken(const QString& token);
    void LoadPaired();
    void SavePaired();

    QWebSocketServer* server = nullptr;
    int port = 0;
    QList<Client> clients;

    QString pairingCode;
    QDateTime pairingCodeExpires;

    QList<QByteArray> pairedHashes;

    quint64 nextRequestId = 1;
    QHash<quint64, Pending> pending;

    State state = State::Stopped;
};
