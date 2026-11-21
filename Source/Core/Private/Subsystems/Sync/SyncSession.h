//
// Created by Stalker7274 on 19.11.2026.
//

#pragma once

#include "sync.qpb.h"

#include <QByteArray>
#include <QObject>
#include <QProtobufSerializer>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class GrpcCall;
class QTimer;

namespace SyncCrypto
{
    class AesGcm;
    class EcdhKey;
}

/// One authenticated, encrypted sync session over a GrpcCall: the handshake described in
/// sync.proto, then sealed Frames both ways, pings and the silence timeout.
/// The session owns its call. Delete it with deleteLater() after closed().
class SyncSession : public QObject
{
    Q_OBJECT

public:
    /// What this device says about itself in Hello.
    struct Identity
    {
        QString deviceId;
        QString deviceName;
        int platform = 1;
        quint16 listenPort = 0;
    };

    enum class Mode
    {
        Sync = 0,  ///< Both sides are already paired and hold the same long-term key.
        Pair = 1   ///< The client proves it knows the code the server shows.
    };

    /// Decisions made during the handshake.
    struct Hooks
    {
        std::function<QByteArray(const QString& peerId)> pskFor;  ///< Long-term key of a paired device; empty: not paired.
        std::function<QString()> pairingCode;                    ///< Server: the code shown now; empty: pairing is closed.
        std::function<void()> badCode;                           ///< Server: a wrong pairing code was tried.
    };

    /// Builds this side's Welcome for the given peer.
    using WelcomeProvider = std::function<soundlink::sync::v1::Welcome(const QString& peerId)>;

    /// @brief Client side: starts the handshake as soon as the call is open.
    /// @param pairingCode Pair mode: the code the user typed. Sync mode takes the key from Hooks::pskFor.
    /// @param expectedPeerId The device that must answer; empty accepts any paired device.
    static SyncSession* Open(GrpcCall* call, const Identity& identity, Mode mode, const QString& pairingCode,
                             const QString& expectedPeerId, Hooks hooks, WelcomeProvider welcome, QObject* parent = nullptr);
    /// @brief Server side, for a call handed over by GrpcServer.
    static SyncSession* Accept(GrpcCall* call, const Identity& identity, Hooks hooks, WelcomeProvider welcome,
                               QObject* parent = nullptr);

    ~SyncSession() override;

    bool IsClient() const { return bClient; }
    bool IsEstablished() const { return stage == Stage::Established; }
    bool IsClosed() const { return stage == Stage::Closed; }
    /// Mode of the handshake; a pairing session continues as a sync session.
    Mode GetMode() const { return mode; }
    QString PeerId() const { return peerId; }
    QString PeerName() const { return peerName; }
    int PeerPlatform() const { return peerPlatform; }
    quint16 PeerListenPort() const { return peerListenPort; }
    QString PeerHost() const { return peerHost; }
    /// The peer's Welcome, valid once established.
    const soundlink::sync::v1::Welcome& PeerWelcome() const { return peerWelcome; }
    /// Pair mode: the new long-term key both sides store for each other.
    QByteArray NewPsk() const { return newPsk; }
    /// Sent bytes still waiting in the transport.
    qint64 PendingBytes() const;

    /// Seals and queues one frame; ignored unless established.
    void SendFrame(const soundlink::sync::v1::Frame& frame);
    /// Ends the session after the queued frames, telling the peer `reason` in a Bye frame.
    /// closed() follows from the event loop.
    void Close(const QString& reason = {});

signals:
    void established();
    void frameReceived(const soundlink::sync::v1::Frame& frame);
    /// Emitted once, always from the event loop. `reason` is the status message of the call
    /// ("bad_code", "not_paired", "pairing_closed", "timeout", ...) or the reason of a Bye.
    void closed(int status, const QString& reason);

private:
    enum class Stage
    {
        WaitOpen,
        WaitHello,
        WaitWelcome,
        Established,
        Closed
    };

    SyncSession(GrpcCall* call, const Identity& identity, bool bClient, WelcomeProvider welcome, QObject* parent);

    void SendHello();
    void OnMessage(const QByteArray& message);
    void OnFinished(int status, const QString& message);
    void HandleHello(const soundlink::sync::v1::Hello& hello);
    void HandleWelcome(const QByteArray& sealed);
    void HandleFrame(const QByteArray& sealed);
    bool DeriveKeys(const QByteArray& peerPublic, const QByteArray& peerNonce, const QByteArray& secret);
    void SendSealed(const soundlink::sync::v1::Frame& frame);
    std::optional<soundlink::sync::v1::Frame> OpenSealed(const QByteArray& sealed);
    void BecomeEstablished();
    void OnPingTimer();
    /// Ends the call with `status`; a server sends it to the client in the trailers.
    void Fail(int status, const QString& reason);
    /// Lets the call finish on its own (it deletes itself) and forgets it.
    GrpcCall* ReleaseCall();
    void EmitClosed(int status, const QString& reason);

    GrpcCall* call = nullptr;
    QTimer* handshakeTimer = nullptr;
    QTimer* pingTimer = nullptr;
    Identity identity;
    WelcomeProvider welcomeProvider;
    Hooks hooks;
    QProtobufSerializer serializer;
    std::unique_ptr<SyncCrypto::EcdhKey> ecdh;
    std::unique_ptr<SyncCrypto::AesGcm> sendKey;
    std::unique_ptr<SyncCrypto::AesGcm> receiveKey;
    QByteArray ownNonce;
    QString clientCode;
    QByteArray newPsk;
    QString expectedPeerId;
    QString peerHost;
    QString peerId;
    QString peerName;
    int peerPlatform = 0;
    quint16 peerListenPort = 0;
    soundlink::sync::v1::Welcome peerWelcome;
    quint64 sendCounter = 0;
    quint64 receiveCounter = 0;
    qint64 lastReceivedMs = 0;
    Stage stage = Stage::WaitOpen;
    Mode mode = Mode::Sync;
    bool bClient = false;
};
