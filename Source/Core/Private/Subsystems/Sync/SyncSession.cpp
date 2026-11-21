//
// Created by Stalker7274 on 19.11.2026.
//

#include "SyncSession.h"

#include "GrpcTransport.h"
#include "SyncCrypto.h"
#include "SyncData.h"

#include <QDateTime>
#include <QTimer>
#include <QtEndian>

namespace proto = soundlink::sync::v1;

namespace
{
    constexpr quint32 ProtocolVersion = 1;
    constexpr int HandshakeTimeoutMs = 15000;
    constexpr int PingIntervalMs = 30000;
    constexpr qint64 SilenceTimeoutMs = 90000;
    constexpr int ReleasedCallTimeoutMs = 10000;

    void AppendLp(QByteArray& out, const QByteArray& value)
    {
        char length[4];
        qToBigEndian<quint32>(quint32(value.size()), length);
        out.append(length, 4);
        out.append(value);
    }

    QByteArray Nonce(quint64 counter)
    {
        QByteArray nonce(12, '\0');
        qToBigEndian<quint64>(counter, nonce.data() + 4);
        return nonce;
    }

    qint64 NowMs()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }
}

SyncSession::SyncSession(GrpcCall* call, const Identity& identity, bool bClient, WelcomeProvider welcome, QObject* parent)
    : QObject(parent)
    , call(call)
    , identity(identity)
    , welcomeProvider(std::move(welcome))
    , bClient(bClient)
{
    call->setParent(this);
    peerHost = call->PeerHost();
    ecdh = std::make_unique<SyncCrypto::EcdhKey>();
    ownNonce = SyncCrypto::RandomBytes(16);
    lastReceivedMs = NowMs();

    handshakeTimer = new QTimer(this);
    handshakeTimer->setSingleShot(true);
    handshakeTimer->setInterval(HandshakeTimeoutMs);
    connect(handshakeTimer, &QTimer::timeout, this, [this]
    {
        Fail(GrpcStatus::Unavailable, QStringLiteral("timeout"));
    });
    handshakeTimer->start();

    pingTimer = new QTimer(this);
    pingTimer->setInterval(PingIntervalMs);
    connect(pingTimer, &QTimer::timeout, this, &SyncSession::OnPingTimer);

    connect(call, &GrpcCall::messageReceived, this, &SyncSession::OnMessage);
    connect(call, &GrpcCall::finished, this, &SyncSession::OnFinished);
}

SyncSession::~SyncSession() = default;

SyncSession* SyncSession::Open(GrpcCall* call, const Identity& identity, Mode mode, const QString& pairingCode,
                               const QString& expectedPeerId, Hooks hooks, WelcomeProvider welcome, QObject* parent)
{
    auto* session = new SyncSession(call, identity, true, std::move(welcome), parent);
    session->mode = mode;
    session->clientCode = pairingCode;
    session->expectedPeerId = expectedPeerId;
    session->hooks = std::move(hooks);
    session->stage = Stage::WaitOpen;
    connect(call, &GrpcCall::opened, session, &SyncSession::SendHello);
    return session;
}

SyncSession* SyncSession::Accept(GrpcCall* call, const Identity& identity, Hooks hooks, WelcomeProvider welcome, QObject* parent)
{
    auto* session = new SyncSession(call, identity, false, std::move(welcome), parent);
    session->hooks = std::move(hooks);
    session->stage = Stage::WaitHello;
    return session;
}

qint64 SyncSession::PendingBytes() const
{
    return call ? call->PendingBytes() : 0;
}

void SyncSession::SendFrame(const proto::Frame& frame)
{
    if (stage == Stage::Established && call)
    {
        SendSealed(frame);
    }
}

void SyncSession::Close(const QString& reason)
{
    if (stage == Stage::Closed)
    {
        return;
    }
    if (stage == Stage::Established && call)
    {
        proto::Bye bye;
        bye.setReason(reason);
        proto::Frame frame;
        frame.setBye(bye);
        SendSealed(frame);
    }
    if (GrpcCall* ending = ReleaseCall())
    {
        ending->Finish(GrpcStatus::Ok);
    }
    EmitClosed(GrpcStatus::Ok, reason);
}

void SyncSession::SendHello()
{
    if (!call || (bClient ? stage != Stage::WaitOpen : stage != Stage::WaitHello))
    {
        return;
    }
    peerHost = call->PeerHost();
    proto::Hello hello;
    hello.setProtocol(ProtocolVersion);
    hello.setDeviceId(identity.deviceId);
    hello.setDeviceName(identity.deviceName);
    hello.setPlatform(static_cast<proto::PlatformGadget::Platform>(identity.platform));
    hello.setMode(static_cast<proto::SessionModeGadget::SessionMode>(static_cast<int>(mode)));
    hello.setEcdhPublic(ecdh->PublicKey());
    hello.setNonce(ownNonce);
    hello.setListenPort(identity.listenPort);
    proto::Envelope envelope;
    envelope.setHello(hello);
    call->Send(envelope.serialize(&serializer));
    if (bClient)
    {
        stage = Stage::WaitHello;
    }
}

void SyncSession::OnMessage(const QByteArray& message)
{
    if (stage == Stage::Closed)
    {
        return;
    }
    lastReceivedMs = NowMs();
    proto::Envelope envelope;
    if (!envelope.deserialize(&serializer, message))
    {
        Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
        return;
    }
    switch (stage)
    {
    case Stage::WaitHello:
        if (!envelope.hasHello())
        {
            Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
            return;
        }
        HandleHello(envelope.hello());
        return;
    case Stage::WaitWelcome:
        if (!envelope.hasSealed())
        {
            Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
            return;
        }
        HandleWelcome(envelope.sealed());
        return;
    case Stage::Established:
        if (!envelope.hasSealed())
        {
            Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
            return;
        }
        HandleFrame(envelope.sealed());
        return;
    default:
        Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
        return;
    }
}

void SyncSession::OnFinished(int status, const QString& message)
{
    if (stage == Stage::Closed)
    {
        return;
    }
    EmitClosed(status, message);
}

void SyncSession::HandleHello(const proto::Hello& hello)
{
    const QByteArray& peerPublic = hello.ecdhPublic();
    const int peerMode = static_cast<int>(hello.mode());
    if (hello.protocol() != ProtocolVersion || !Sync::IsDeviceId(hello.deviceId()) || hello.deviceId() == identity.deviceId
        || peerPublic.size() != 65 || hello.nonce().size() != 16 || (peerMode != 0 && peerMode != 1) || hello.listenPort() > 65535)
    {
        Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
        return;
    }
    peerId = hello.deviceId();
    peerName = hello.deviceName().left(128);
    peerPlatform = static_cast<int>(hello.platform());
    peerListenPort = quint16(hello.listenPort());

    QByteArray secret;
    if (bClient)
    {
        if (peerMode != static_cast<int>(mode))
        {
            Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
            return;
        }
        if (!expectedPeerId.isEmpty() && peerId != expectedPeerId)
        {
            Fail(GrpcStatus::Unauthenticated, QStringLiteral("wrong_peer"));
            return;
        }
        if (mode == Mode::Pair)
        {
            secret = clientCode.toUtf8();
        }
        else
        {
            secret = hooks.pskFor ? hooks.pskFor(peerId) : QByteArray();
            if (secret.isEmpty())
            {
                Fail(GrpcStatus::Unauthenticated, QStringLiteral("not_paired"));
                return;
            }
        }
    }
    else
    {
        mode = static_cast<Mode>(peerMode);
        if (mode == Mode::Sync)
        {
            secret = hooks.pskFor ? hooks.pskFor(peerId) : QByteArray();
            if (secret.isEmpty())
            {
                Fail(GrpcStatus::Unauthenticated, QStringLiteral("not_paired"));
                return;
            }
        }
        else
        {
            const QString code = hooks.pairingCode ? hooks.pairingCode() : QString();
            if (code.isEmpty())
            {
                Fail(GrpcStatus::FailedPrecondition, QStringLiteral("pairing_closed"));
                return;
            }
            secret = code.toUtf8();
        }
        SendHello();
    }

    if (!DeriveKeys(peerPublic, hello.nonce(), secret))
    {
        Fail(GrpcStatus::Internal, QStringLiteral("crypto"));
        return;
    }
    stage = Stage::WaitWelcome;
    if (bClient)
    {
        proto::Frame frame;
        frame.setWelcome(welcomeProvider(peerId));
        SendSealed(frame);
    }
}

void SyncSession::HandleWelcome(const QByteArray& sealed)
{
    const std::optional<proto::Frame> frame = OpenSealed(sealed);
    if (!frame)
    {
        if (bClient)
        {
            Fail(GrpcStatus::Unauthenticated, QStringLiteral("bad_key"));
        }
        else if (mode == Mode::Pair)
        {
            if (hooks.badCode)
            {
                hooks.badCode();
            }
            Fail(GrpcStatus::Unauthenticated, QStringLiteral("bad_code"));
        }
        else
        {
            Fail(GrpcStatus::Unauthenticated, QStringLiteral("not_paired"));
        }
        return;
    }
    if (!frame->hasWelcome())
    {
        Fail(GrpcStatus::FailedPrecondition, QStringLiteral("protocol"));
        return;
    }
    peerWelcome = frame->welcome();
    if (!peerWelcome.deviceName().isEmpty())
    {
        peerName = peerWelcome.deviceName().left(128);
    }
    if (!bClient)
    {
        proto::Frame own;
        own.setWelcome(welcomeProvider(peerId));
        SendSealed(own);
    }
    BecomeEstablished();
}

void SyncSession::HandleFrame(const QByteArray& sealed)
{
    const std::optional<proto::Frame> frame = OpenSealed(sealed);
    if (!frame)
    {
        Fail(GrpcStatus::Unauthenticated, QStringLiteral("bad_frame"));
        return;
    }
    if (frame->hasPing())
    {
        proto::Pong pong;
        pong.setTimeMs(frame->ping().timeMs());
        proto::Frame reply;
        reply.setPong(pong);
        SendSealed(reply);
        return;
    }
    if (frame->hasPong() || frame->hasWelcome())
    {
        return;
    }
    if (frame->hasBye())
    {
        const QString reason = frame->bye().reason().left(64);
        emit frameReceived(*frame);
        if (stage == Stage::Closed)
        {
            return;
        }
        if (GrpcCall* ending = ReleaseCall())
        {
            ending->Finish(GrpcStatus::Ok);
        }
        EmitClosed(GrpcStatus::Ok, reason);
        return;
    }
    emit frameReceived(*frame);
}

bool SyncSession::DeriveKeys(const QByteArray& peerPublic, const QByteArray& peerNonce, const QByteArray& secret)
{
    const QByteArray shared = ecdh->Agree(peerPublic);
    if (shared.size() != 32 || secret.isEmpty())
    {
        return false;
    }
    const QByteArray ownPublic = ecdh->PublicKey();
    QByteArray transcript("SoundLink sync v1");
    transcript.append(char(static_cast<int>(mode)));
    AppendLp(transcript, (bClient ? identity.deviceId : peerId).toUtf8());
    AppendLp(transcript, bClient ? ownPublic : peerPublic);
    AppendLp(transcript, bClient ? ownNonce : peerNonce);
    AppendLp(transcript, (bClient ? peerId : identity.deviceId).toUtf8());
    AppendLp(transcript, bClient ? peerPublic : ownPublic);
    AppendLp(transcript, bClient ? peerNonce : ownNonce);
    const QByteArray th = SyncCrypto::Sha256(transcript);

    const QByteArray key = mode == Mode::Pair ? SyncCrypto::Sha256(QByteArray("SoundLink pair v1") + secret) : secret;
    const QByteArray prk = SyncCrypto::HmacSha256(key, shared + th);
    const QByteArray c2s = SyncCrypto::HmacSha256(prk, QByteArray("c2s\x01", 4));
    const QByteArray s2c = SyncCrypto::HmacSha256(prk, QByteArray("s2c\x01", 4));
    if (mode == Mode::Pair)
    {
        newPsk = SyncCrypto::HmacSha256(prk, QByteArray("psk\x01", 4));
    }
    sendKey = std::make_unique<SyncCrypto::AesGcm>(bClient ? c2s : s2c);
    receiveKey = std::make_unique<SyncCrypto::AesGcm>(bClient ? s2c : c2s);
    ecdh.reset();
    return sendKey->IsValid() && receiveKey->IsValid();
}

void SyncSession::SendSealed(const proto::Frame& frame)
{
    if (!call || !sendKey)
    {
        return;
    }
    proto::Envelope envelope;
    envelope.setSealed(sendKey->Seal(Nonce(sendCounter++), frame.serialize(&serializer)));
    call->Send(envelope.serialize(&serializer));
}

std::optional<proto::Frame> SyncSession::OpenSealed(const QByteArray& sealed)
{
    if (!receiveKey)
    {
        return std::nullopt;
    }
    const std::optional<QByteArray> plain = receiveKey->Open(Nonce(receiveCounter), sealed);
    if (!plain)
    {
        return std::nullopt;
    }
    ++receiveCounter;
    proto::Frame frame;
    if (!frame.deserialize(&serializer, *plain))
    {
        return std::nullopt;
    }
    return frame;
}

void SyncSession::BecomeEstablished()
{
    stage = Stage::Established;
    handshakeTimer->stop();
    pingTimer->start();
    if (call)
    {
        peerHost = call->PeerHost();
    }
    emit established();
}

void SyncSession::OnPingTimer()
{
    if (stage != Stage::Established)
    {
        return;
    }
    if (NowMs() - lastReceivedMs > SilenceTimeoutMs)
    {
        Fail(GrpcStatus::Unavailable, QStringLiteral("timeout"));
        return;
    }
    proto::Ping ping;
    ping.setTimeMs(quint64(NowMs()));
    proto::Frame frame;
    frame.setPing(ping);
    SendSealed(frame);
}

void SyncSession::Fail(int status, const QString& reason)
{
    if (stage == Stage::Closed)
    {
        return;
    }
    if (GrpcCall* ending = ReleaseCall())
    {
        if (bClient)
        {
            ending->Abort();
        }
        else
        {
            ending->Finish(status, reason);
        }
    }
    EmitClosed(status, reason);
}

GrpcCall* SyncSession::ReleaseCall()
{
    GrpcCall* released = call;
    if (!released)
    {
        return nullptr;
    }
    call = nullptr;
    disconnect(released, nullptr, this, nullptr);
    released->setParent(nullptr);
    if (released->IsFinished())
    {
        released->deleteLater();
    }
    else
    {
        connect(released, &GrpcCall::finished, released, &QObject::deleteLater);
        QTimer::singleShot(ReleasedCallTimeoutMs, released, &GrpcCall::Abort);
    }
    return released;
}

void SyncSession::EmitClosed(int status, const QString& reason)
{
    if (stage == Stage::Closed)
    {
        return;
    }
    stage = Stage::Closed;
    handshakeTimer->stop();
    pingTimer->stop();
    QMetaObject::invokeMethod(this, [this, status, reason]
    {
        emit closed(status, reason);
    }, Qt::QueuedConnection);
}
