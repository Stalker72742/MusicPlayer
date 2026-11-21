//
// Created by Stalker7274 on 19.11.2026.
//

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QBuffer;
class QHttp2Connection;
class QHttp2Stream;
class QTcpServer;
class QTcpSocket;
class QTimer;

/// gRPC status codes used by the sync protocol.
namespace GrpcStatus
{
    inline constexpr int Ok = 0;
    inline constexpr int Cancelled = 1;
    inline constexpr int Unknown = 2;
    inline constexpr int ResourceExhausted = 8;
    inline constexpr int FailedPrecondition = 9;
    inline constexpr int Unimplemented = 12;
    inline constexpr int Internal = 13;
    inline constexpr int Unavailable = 14;
    inline constexpr int Unauthenticated = 16;
}

/// One bidirectional-streaming gRPC call carried by its own plaintext HTTP/2 connection
/// (h2c with prior knowledge), client or server side. QtGrpc 6.8 has no server, so both
/// sides are built on Qt's internal QHttp2Connection (Qt6::NetworkPrivate).
/// Delete it with deleteLater(), never from inside one of its own signals.
class GrpcCall : public QObject
{
    Q_OBJECT

public:
    /// Largest message accepted from the peer.
    static constexpr qsizetype MaxMessageSize = 8 * 1024 * 1024;

    /// Starts a client call: connects to host:port and opens the stream at `path`
    /// ("/package.Service/Method"). The call then emits opened() or finished().
    static GrpcCall* Connect(const QString& host, quint16 port, const QByteArray& path, QObject* parent = nullptr);

    ~GrpcCall() override;

    bool IsClient() const { return bClient; }
    bool IsFinished() const { return bEnded; }
    /// Remote address of the TCP connection, IPv4-mapped addresses unwrapped.
    QString PeerHost() const { return peerHost; }
    quint16 PeerPort() const { return peerPort; }
    /// Bytes queued for sending that have not reached the socket yet, plus the socket's own buffer.
    qint64 PendingBytes() const;

    /// Queues one message.
    void Send(const QByteArray& message);
    /// Ends the call after the queued messages: a client half-closes and waits for the status,
    /// a server sends `status` in its trailers.
    void Finish(int status = GrpcStatus::Ok, const QString& message = {});
    /// Ends the call at once with RST_STREAM and drops the connection; emits finished(Cancelled).
    void Abort();

signals:
    /// Client side: the stream is open. Server calls are already open when GrpcServer hands them over.
    void opened();
    void messageReceived(const QByteArray& message);
    /// The call is over; emitted exactly once. Transport failures report GrpcStatus::Unavailable.
    void finished(int status, const QString& message);

private:
    friend class GrpcServer;

    GrpcCall(QTcpSocket* socket, bool bClient, QObject* parent);

    void OnConnected();
    void AttachStream(QHttp2Stream* newStream);
    void HandleHeaders(const QHash<QByteArray, QByteArray>& headers, bool bEndStream);
    void HandleData(const QByteArray& data, bool bEndStream);
    void HandleRemoteHalfClose();
    void HandleUploadFinished();
    void Pump();
    void SendFinish();
    void EndCall(int status, const QString& message, bool bReset);

    QTcpSocket* socket = nullptr;
    QHttp2Connection* connection = nullptr;
    QHttp2Stream* stream = nullptr;
    QBuffer* uploading = nullptr;
    QTimer* timeout = nullptr;
    QByteArray path;
    QByteArray authority;
    QByteArray sendQueue;
    QByteArray receiveBuffer;
    qint64 uploadedBytes = 0;
    QString peerHost;
    quint16 peerPort = 0;
    int finishStatus = GrpcStatus::Ok;
    QString finishMessage;
    bool bClient = false;
    bool bAccepted = false;
    bool bHeadersSent = false;
    bool bHeadersReceived = false;
    bool bFinishRequested = false;
    bool bHalfClosedLocal = false;
    bool bEnded = false;
};

/// Accepts h2c connections and hands every call on the served method path over as a GrpcCall.
class GrpcServer : public QObject
{
    Q_OBJECT

public:
    explicit GrpcServer(const QByteArray& path, QObject* parent = nullptr);

    /// Listens on every interface, IPv4 and IPv6.
    bool Listen(quint16 port);
    void Close();
    bool IsListening() const;
    quint16 Port() const;
    QString ErrorString() const;

signals:
    /// A client opened a call on the served path. The receiver owns it from now on.
    void incomingCall(GrpcCall* call);

private:
    void OnNewConnection();

    QTcpServer* server = nullptr;
    QByteArray path;
};
