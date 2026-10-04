//
// Created by Stalker7274 on 04.10.2026.
//

#include "Online/BrowserBridge.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketCorsAuthenticator>
#include <QWebSocketServer>

namespace
{
    constexpr int FirstPort = 47821;
    constexpr int PortCount = 5;

    constexpr int ProtocolVersion = 1;

    constexpr int MaxClients = 6;
    constexpr int MaxUnpairedClients = 3;
    constexpr qint64 MaxMessageSize = 1 << 20;

    constexpr int HelloTimeoutMs = 10'000;
    constexpr int PairingTimeoutMs = 10 * 60'000;
    constexpr int IdleTimeoutMs = 75'000;

    constexpr int PairingCodeLifetimeSec = 10 * 60;
    constexpr int MaxPairAttempts = 5;

    bool IsExtensionOrigin(const QString& origin)
    {
        return origin.startsWith(QStringLiteral("chrome-extension://")) || origin.startsWith(QStringLiteral("moz-extension://"));
    }

    QString GetStorePath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/browser-bridge.json";
    }
}

BrowserBridge::BrowserBridge()
{
    LoadPaired();
}

void BrowserBridge::Deinitialize()
{
    for (Pending& request : pending)
        delete request.timer;
    pending.clear();

    for (Client& client : clients)
    {
        client.socket->disconnect(this);
        client.socket->abort();
        delete client.socket;
    }
    clients.clear();

    delete server;
    server = nullptr;
}

void BrowserBridge::Start()
{
    if (server)
        return;

    server = new QWebSocketServer(QStringLiteral("SoundLink"), QWebSocketServer::NonSecureMode, this);
    server->setMaxPendingConnections(MaxClients);

    connect(server, &QWebSocketServer::originAuthenticationRequired, this,
        [](QWebSocketCorsAuthenticator* authenticator) { authenticator->setAllowed(IsExtensionOrigin(authenticator->origin())); });
    connect(server, &QWebSocketServer::newConnection, this, &BrowserBridge::OnNewConnection);

    connect(server, &QWebSocketServer::serverError, this, [](QWebSocketProtocol::CloseCode code) {
        qWarning() << "[BrowserBridge] A connection failed the WebSocket handshake:" << code;
    });

    for (int candidate = FirstPort; candidate < FirstPort + PortCount; ++candidate)
    {
        if (server->listen(QHostAddress::LocalHost, static_cast<quint16>(candidate)))
        {
            port = candidate;
            break;
        }
    }

    if (!port)
    {
        qWarning() << "[BrowserBridge] No free port in" << FirstPort << "-" << FirstPort + PortCount - 1;
        UpdateState();
        return;
    }

    qDebug() << "[BrowserBridge] Listening on 127.0.0.1:" << port;
    UpdateState();
}

void BrowserBridge::OnNewConnection()
{
    while (QWebSocket* socket = server->nextPendingConnection())
    {
        socket->setParent(this);

        const qsizetype unpaired = std::count_if(clients.cbegin(), clients.cend(), [](const Client& c) { return !c.bPaired; });
        if (!IsExtensionOrigin(socket->origin()) || clients.size() >= MaxClients || unpaired >= MaxUnpairedClients)
        {
            qWarning() << "[BrowserBridge] Refused a connection from" << socket->origin();
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated);
            socket->deleteLater();
            continue;
        }

        socket->setMaxAllowedIncomingMessageSize(MaxMessageSize);
        socket->setMaxAllowedIncomingFrameSize(MaxMessageSize);

        Client client;
        client.socket = socket;
        client.deadline = new QTimer(socket);
        client.deadline->setSingleShot(true);
        client.lastSeen = QDateTime::currentDateTime();
        connect(client.deadline, &QTimer::timeout, this, [this, socket] { Drop(socket, QStringLiteral("timed out")); });
        client.deadline->start(HelloTimeoutMs);

        connect(socket, &QWebSocket::textMessageReceived, this,
            [this, socket](const QString& message) { OnMessage(socket, message); });
        connect(socket, &QWebSocket::binaryMessageReceived, this,
            [this, socket](const QByteArray&) { Drop(socket, QStringLiteral("binary frames are not used")); });
        connect(socket, &QWebSocket::disconnected, this, [this, socket] { OnDisconnected(socket); });

        clients.append(client);
    }
}

void BrowserBridge::OnMessage(QWebSocket* socket, const QString& text)
{
    Client* client = FindClient(socket);
    if (!client)
        return;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (!document.isObject())
    {
        Drop(socket, QStringLiteral("malformed message"));
        return;
    }

    const QJsonObject message = document.object();
    const QString type = message.value("type").toString();
    client->lastSeen = QDateTime::currentDateTime();

    if (type == QStringLiteral("hello"))
    {
        HandleHello(*client, message);
        return;
    }
    if (!client->bHello)
    {
        Drop(socket, QStringLiteral("no hello"));
        return;
    }
    if (type == QStringLiteral("pair"))
    {
        HandlePair(*client, message);
        return;
    }
    if (type == QStringLiteral("ping"))
    {
        if (client->bPaired)
            client->deadline->start(IdleTimeoutMs);
        Send(socket, {{"type", "pong"}});
        return;
    }

    if (!client->bPaired)
    {
        Drop(socket, QStringLiteral("not paired"));
        return;
    }

    client->deadline->start(IdleTimeoutMs);
    if (type == QStringLiteral("response"))
        HandleResponse(message);
}

void BrowserBridge::HandleHello(Client& client, const QJsonObject& message)
{
    if (client.bHello)
        return;

    client.bHello = true;
    client.browser = message.value("browser").toString().left(64);

    const QString token = message.value("token").toString();
    client.bPaired = !token.isEmpty() && pairedHashes.contains(HashToken(token));

    Send(client.socket, {{"type", "welcome"}, {"app", "SoundLink"}, {"protocol", ProtocolVersion}, {"paired", client.bPaired}});

    if (client.bPaired)
    {
        qDebug() << "[BrowserBridge] Extension connected:" << client.browser;
        client.deadline->start(IdleTimeoutMs);
    }
    else
    {
        EnsurePairingCode();
        client.deadline->start(PairingTimeoutMs);
    }
    UpdateState();
}

void BrowserBridge::HandlePair(Client& client, const QJsonObject& message)
{
    if (client.bPaired)
        return;

    EnsurePairingCode();
    const QString code = message.value("code").toString().trimmed();

    if (code.isEmpty() || code != pairingCode)
    {
        ++client.pairAttempts;
        const int attemptsLeft = MaxPairAttempts - client.pairAttempts;
        Send(client.socket, {{"type", "pairFailed"}, {"attemptsLeft", attemptsLeft}});

        if (attemptsLeft <= 0)
        {
            pairingCode.clear();
            Drop(client.socket, QStringLiteral("too many pairing attempts"));
        }
        return;
    }

    QByteArray random(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(random.data()), random.size() / 4);
    const QString token = QString::fromLatin1(random.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    pairedHashes.append(HashToken(token));
    SavePaired();

    client.bPaired = true;
    client.deadline->start(IdleTimeoutMs);
    pairingCode.clear();

    Send(client.socket, {{"type", "paired"}, {"token", token}});
    qDebug() << "[BrowserBridge] Paired with" << client.browser;
    UpdateState();
}

void BrowserBridge::HandleResponse(const QJsonObject& message)
{
    const quint64 id = static_cast<quint64>(message.value("id").toInteger());
    const auto it = pending.find(id);
    if (it == pending.end())
        return;

    const Pending request = it.value();
    pending.erase(it);
    delete request.timer;

    if (!request.context)
        return;

    const QString error = message.value("error").toString();
    if (!error.isEmpty())
        request.callback({}, error);
    else
        request.callback(message.value("result").toObject(), {});
}

void BrowserBridge::Request(const QString& method, const QJsonObject& params, QObject* context,
    ResponseCallback callback, int timeoutMs)
{
    Client* client = ActiveClient();
    if (!client)
    {
        QTimer::singleShot(0, context, [callback] { callback({}, QStringLiteral("The browser extension is not connected")); });
        return;
    }

    const quint64 id = nextRequestId++;

    Pending request;
    request.context = context;
    request.callback = std::move(callback);
    request.socket = client->socket;
    request.timer = new QTimer(this);
    request.timer->setSingleShot(true);
    connect(request.timer, &QTimer::timeout, this, [this, id] {
        const auto it = pending.find(id);
        if (it == pending.end())
            return;

        const Pending expired = it.value();
        pending.erase(it);
        expired.timer->deleteLater();
        if (expired.context)
            expired.callback({}, QStringLiteral("The browser extension did not answer in time"));
    });
    request.timer->start(timeoutMs);
    pending.insert(id, request);

    Send(client->socket, {{"type", "request"}, {"id", static_cast<qint64>(id)}, {"method", method}, {"params", params}});
}

void BrowserBridge::Send(QWebSocket* socket, const QJsonObject& message)
{
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void BrowserBridge::Drop(QWebSocket* socket, const QString& reason)
{
    if (!FindClient(socket))
        return;

    qDebug() << "[BrowserBridge] Dropping a connection:" << reason;
    socket->close(QWebSocketProtocol::CloseCodePolicyViolated, reason);
    OnDisconnected(socket);
}

void BrowserBridge::OnDisconnected(QWebSocket* socket)
{
    const auto it = std::find_if(clients.begin(), clients.end(), [socket](const Client& c) { return c.socket == socket; });
    if (it == clients.end())
        return;

    clients.erase(it);
    socket->disconnect(this);
    socket->deleteLater();

    FailPending(socket, QStringLiteral("The browser extension disconnected"));
    UpdateState();
}

void BrowserBridge::FailPending(QWebSocket* socket, const QString& error)
{
    QList<Pending> failed;
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->socket == socket)
        {
            failed.append(it.value());
            it = pending.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const Pending& request : failed)
    {
        delete request.timer;
        if (request.context)
            request.callback({}, error);
    }
}

BrowserBridge::Client* BrowserBridge::FindClient(QWebSocket* socket)
{
    for (Client& client : clients)
    {
        if (client.socket == socket)
            return &client;
    }
    return nullptr;
}

BrowserBridge::Client* BrowserBridge::ActiveClient()
{
    Client* active = nullptr;
    for (Client& client : clients)
    {
        if (client.bPaired && (!active || client.lastSeen > active->lastSeen))
            active = &client;
    }
    return active;
}

QString BrowserBridge::GetBrowserName() const
{
    for (const Client& client : clients)
    {
        if (client.bPaired)
            return client.browser;
    }
    return {};
}

void BrowserBridge::EnsurePairingCode()
{
    if (!pairingCode.isEmpty() && QDateTime::currentDateTime() < pairingCodeExpires)
        return;

    pairingCode = QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(1'000'000), 6, 10, QLatin1Char('0'));
    pairingCodeExpires = QDateTime::currentDateTime().addSecs(PairingCodeLifetimeSec);
    UpdateState();
}

void BrowserBridge::UpdateState()
{
    State newState = State::Stopped;
    if (port)
    {
        const bool bPaired = std::any_of(clients.cbegin(), clients.cend(), [](const Client& c) { return c.bPaired; });
        const bool bWaiting = std::any_of(clients.cbegin(), clients.cend(), [](const Client& c) { return c.bHello && !c.bPaired; });
        newState = bPaired ? State::Connected : (bWaiting ? State::PairingRequired : State::Waiting);
    }

    state = newState;
    emit stateChanged();
}

void BrowserBridge::ForgetPairedBrowsers()
{
    pairedHashes.clear();
    SavePaired();

    QList<QWebSocket*> paired;
    for (const Client& client : clients)
    {
        if (client.bPaired)
            paired.append(client.socket);
    }
    for (QWebSocket* socket : paired)
        Drop(socket, QStringLiteral("pairing revoked"));

    UpdateState();
}

QByteArray BrowserBridge::HashToken(const QString& token)
{
    return QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex();
}

void BrowserBridge::LoadPaired()
{
    QFile file(GetStorePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    for (const QJsonValue& value : QJsonDocument::fromJson(file.readAll()).object().value("tokens").toArray())
    {
        const QByteArray hash = value.toString().toLatin1();
        if (hash.size() == 64)
            pairedHashes.append(hash);
    }
}

void BrowserBridge::SavePaired()
{
    QJsonArray tokens;
    for (const QByteArray& hash : pairedHashes)
        tokens.append(QString::fromLatin1(hash));

    const QString path = GetStorePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "[BrowserBridge] Cannot save pairings:" << file.errorString();
        return;
    }
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"tokens", tokens}}).toJson());
    if (!file.commit())
        qWarning() << "[BrowserBridge] Cannot save pairings:" << file.errorString();
}
