//
// Created by Stalker7274 on 19.11.2026.
//

#include "GrpcTransport.h"

#include <QBuffer>
#include <QHostAddress>
#include <QHttp2Configuration>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

#include <QtNetwork/private/qhttp2connection_p.h>

#include <initializer_list>
#include <utility>

namespace
{
    constexpr int ConnectTimeoutMs = 10000;
    constexpr int RequestTimeoutMs = 10000;
    constexpr int StatusTimeoutMs = 5000;
    constexpr int LingerMs = 5000;

    QHttp2Configuration Configuration()
    {
        QHttp2Configuration config;
        config.setServerPushEnabled(false);
        config.setHuffmanCompressionEnabled(true);
        config.setSessionReceiveWindowSize(16 * 1024 * 1024);
        config.setStreamReceiveWindowSize(8 * 1024 * 1024);
        return config;
    }

    QHash<QByteArray, QByteArray> ToMap(const HPack::HttpHeader& headers)
    {
        QHash<QByteArray, QByteArray> map;
        for (const HPack::HeaderField& field : headers)
        {
            map.insert(field.name.toLower(), field.value);
        }
        return map;
    }

    HPack::HttpHeader Fields(std::initializer_list<std::pair<QByteArray, QByteArray>> list)
    {
        HPack::HttpHeader headers;
        headers.reserve(list.size());
        for (const auto& [name, value] : list)
        {
            headers.emplace_back(name, value);
        }
        return headers;
    }

    QByteArray EncodeGrpcMessage(const QString& message)
    {
        static constexpr char Hex[] = "0123456789ABCDEF";
        QByteArray out;
        for (const char c : message.toUtf8())
        {
            const uchar u = uchar(c);
            if (u >= 0x20 && u <= 0x7e && u != '%')
            {
                out.append(c);
            }
            else
            {
                out.append('%');
                out.append(Hex[u >> 4]);
                out.append(Hex[u & 0x0f]);
            }
        }
        return out;
    }

    QString CleanHost(const QHostAddress& address)
    {
        bool bIsV4 = false;
        const quint32 v4 = address.toIPv4Address(&bIsV4);
        return bIsV4 ? QHostAddress(v4).toString() : address.toString();
    }

    void ConfigureSocket(QTcpSocket* socket)
    {
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    }
}

GrpcCall::GrpcCall(QTcpSocket* socket, bool bClient, QObject* parent)
    : QObject(parent)
    , socket(socket)
    , bClient(bClient)
{
    socket->setParent(this);
    timeout = new QTimer(this);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this]
    {
        EndCall(GrpcStatus::Unavailable, QStringLiteral("timed out"), true);
    });
    connect(socket, &QAbstractSocket::errorOccurred, this, [this]
    {
        if (this->socket)
        {
            EndCall(GrpcStatus::Unavailable, this->socket->errorString(), false);
        }
    });
    connect(socket, &QAbstractSocket::disconnected, this, [this]
    {
        if (connection)
        {
            connection->handleConnectionClosure();
        }
        EndCall(GrpcStatus::Unavailable, QStringLiteral("connection closed"), false);
    });
}

GrpcCall::~GrpcCall()
{
    bEnded = true;
    if (socket)
    {
        disconnect(socket, nullptr, this, nullptr);
        socket->abort();
    }
}

GrpcCall* GrpcCall::Connect(const QString& host, quint16 port, const QByteArray& path, QObject* parent)
{
    auto* call = new GrpcCall(new QTcpSocket(), true, parent);
    call->path = path;
    const bool bIpv6 = QHostAddress(host).protocol() == QAbstractSocket::IPv6Protocol;
    call->authority = QStringLiteral("%1:%2").arg(bIpv6 ? QStringLiteral("[%1]").arg(host) : host).arg(port).toUtf8();
    call->peerHost = host;
    call->peerPort = port;
    connect(call->socket, &QAbstractSocket::connected, call, &GrpcCall::OnConnected);
    call->timeout->start(ConnectTimeoutMs);
    QMetaObject::invokeMethod(call, [call, host, port]
    {
        if (!call->bEnded && call->socket)
        {
            call->socket->connectToHost(host, port);
        }
    }, Qt::QueuedConnection);
    return call;
}

qint64 GrpcCall::PendingBytes() const
{
    qint64 pending = sendQueue.size();
    if (uploading)
    {
        pending += qMax<qint64>(0, uploading->size() - uploadedBytes);
    }
    if (socket)
    {
        pending += socket->bytesToWrite();
    }
    return pending;
}

void GrpcCall::Send(const QByteArray& message)
{
    if (bEnded || bFinishRequested || bHalfClosedLocal)
    {
        return;
    }
    char prefix[5];
    prefix[0] = 0;
    qToBigEndian<quint32>(quint32(message.size()), prefix + 1);
    sendQueue.append(prefix, 5);
    sendQueue.append(message);
    Pump();
}

void GrpcCall::Finish(int status, const QString& message)
{
    if (bEnded || bFinishRequested || bHalfClosedLocal)
    {
        return;
    }
    if (!stream)
    {
        EndCall(status == GrpcStatus::Ok ? GrpcStatus::Cancelled : status, message, true);
        return;
    }
    finishStatus = status;
    finishMessage = message;
    bFinishRequested = true;
    Pump();
}

void GrpcCall::Abort()
{
    EndCall(GrpcStatus::Cancelled, QStringLiteral("aborted"), true);
}

void GrpcCall::OnConnected()
{
    if (bEnded || !socket)
    {
        return;
    }
    timeout->stop();
    ConfigureSocket(socket);
    peerHost = CleanHost(socket->peerAddress());
    connection = QHttp2Connection::createDirectConnection(socket, Configuration());
    if (!connection)
    {
        EndCall(GrpcStatus::Unavailable, QStringLiteral("HTTP/2 setup failed"), false);
        return;
    }
    connect(socket, &QIODevice::readyRead, connection, &QHttp2Connection::handleReadyRead);
    connect(connection, &QHttp2Connection::errorOccurred, this, [this](Http2::Http2Error, const QString& message)
    {
        EndCall(GrpcStatus::Unavailable, message, false);
    });
    const auto created = connection->createStream();
    if (!created.ok())
    {
        EndCall(GrpcStatus::Unavailable, QStringLiteral("cannot open an HTTP/2 stream"), false);
        return;
    }
    AttachStream(created.unwrap());
    const HPack::HttpHeader headers = Fields({
        {":method", "POST"},
        {":scheme", "http"},
        {":path", path},
        {":authority", authority},
        {"content-type", "application/grpc"},
        {"te", "trailers"},
        {"user-agent", "soundlink-qt/1"},
    });
    if (!stream->sendHEADERS(headers, false))
    {
        EndCall(GrpcStatus::Internal, QStringLiteral("cannot send request headers"), true);
        return;
    }
    QPointer<GrpcCall> self(this);
    emit opened();
    if (self)
    {
        Pump();
    }
}

void GrpcCall::AttachStream(QHttp2Stream* newStream)
{
    stream = newStream;
    connect(stream, &QHttp2Stream::headersReceived, this, [this](const HPack::HttpHeader& headers, bool bEndStream)
    {
        HandleHeaders(ToMap(headers), bEndStream);
    });
    connect(stream, &QHttp2Stream::dataReceived, this, [this](const QByteArray& data, bool bEndStream)
    {
        if (stream)
        {
            stream->clearDownloadBuffer();
        }
        HandleData(data, bEndStream);
    });
    connect(stream, &QHttp2Stream::errorOccurred, this, [this](Http2::Http2Error code, const QString& message)
    {
        EndCall(code == Http2::CANCEL ? GrpcStatus::Cancelled : GrpcStatus::Unavailable,
                message.isEmpty() ? QStringLiteral("stream error %1").arg(int(code)) : message, false);
    });
    connect(stream, &QHttp2Stream::bytesWritten, this, [this](qint64 bytes)
    {
        uploadedBytes += bytes;
    });
    connect(stream, &QHttp2Stream::uploadFinished, this, &GrpcCall::HandleUploadFinished);
}

void GrpcCall::HandleHeaders(const QHash<QByteArray, QByteArray>& headers, bool bEndStream)
{
    if (bEnded)
    {
        return;
    }
    if (!bClient)
    {
        if (!bHeadersReceived)
        {
            bHeadersReceived = true;
            const bool bValid = headers.value(":method") == "POST" && headers.value(":path") == path
                                && headers.value("content-type").startsWith("application/grpc");
            if (!bValid)
            {
                Finish(GrpcStatus::Unimplemented, QStringLiteral("unknown method"));
                return;
            }
            timeout->stop();
            bAccepted = true;
            if (auto* server = qobject_cast<GrpcServer*>(parent()))
            {
                QPointer<GrpcCall> self(this);
                emit server->incomingCall(this);
                if (!self || bEnded)
                {
                    return;
                }
            }
        }
        if (bEndStream)
        {
            HandleRemoteHalfClose();
        }
        return;
    }

    if (!bHeadersReceived)
    {
        bHeadersReceived = true;
        const QByteArray status = headers.value(":status");
        if (status != "200")
        {
            EndCall(GrpcStatus::Unavailable, QStringLiteral("HTTP status %1").arg(QString::fromLatin1(status)), true);
            return;
        }
        if (!bEndStream && !headers.contains("grpc-status"))
        {
            if (!headers.value("content-type").startsWith("application/grpc"))
            {
                EndCall(GrpcStatus::Unknown, QStringLiteral("not a gRPC server"), true);
            }
            return;
        }
    }
    if (headers.contains("grpc-status") || bEndStream)
    {
        bool bOk = false;
        const int code = headers.value("grpc-status").toInt(&bOk);
        const QString message = QString::fromUtf8(QByteArray::fromPercentEncoding(headers.value("grpc-message")));
        EndCall(bOk ? code : GrpcStatus::Unknown, bOk ? message : QStringLiteral("missing grpc-status"), !bEndStream);
    }
}

void GrpcCall::HandleData(const QByteArray& data, bool bEndStream)
{
    if (bEnded)
    {
        return;
    }
    receiveBuffer.append(data);
    QPointer<GrpcCall> self(this);
    qsizetype offset = 0;
    while (receiveBuffer.size() - offset >= 5)
    {
        const char* header = receiveBuffer.constData() + offset;
        if (header[0] != 0)
        {
            EndCall(GrpcStatus::Unimplemented, QStringLiteral("compressed messages are not supported"), true);
            return;
        }
        const quint32 length = qFromBigEndian<quint32>(header + 1);
        if (length > quint32(MaxMessageSize))
        {
            EndCall(GrpcStatus::ResourceExhausted, QStringLiteral("message too large"), true);
            return;
        }
        if (receiveBuffer.size() - offset - 5 < qsizetype(length))
        {
            break;
        }
        const QByteArray message = receiveBuffer.mid(offset + 5, length);
        offset += 5 + qsizetype(length);
        emit messageReceived(message);
        if (!self || bEnded)
        {
            return;
        }
    }
    receiveBuffer.remove(0, offset);
    if (bEndStream)
    {
        HandleRemoteHalfClose();
    }
}

void GrpcCall::HandleRemoteHalfClose()
{
    if (bEnded)
    {
        return;
    }
    if (bClient)
    {
        EndCall(GrpcStatus::Unknown, QStringLiteral("the server ended the stream without a status"), false);
        return;
    }
    Finish(GrpcStatus::Ok, {});
}

void GrpcCall::HandleUploadFinished()
{
    if (uploading)
    {
        uploading->deleteLater();
        uploading = nullptr;
    }
    uploadedBytes = 0;
    if (!bHalfClosedLocal)
    {
        Pump();
    }
}

void GrpcCall::Pump()
{
    if (bEnded || !stream || uploading)
    {
        return;
    }
    const QHttp2Stream::State state = stream->state();
    if (state != QHttp2Stream::State::Open && state != QHttp2Stream::State::HalfClosedRemote)
    {
        return;
    }
    if (!sendQueue.isEmpty())
    {
        if (!bClient && !bHeadersSent)
        {
            bHeadersSent = true;
            if (!stream->sendHEADERS(Fields({{":status", "200"}, {"content-type", "application/grpc"}}), false))
            {
                EndCall(GrpcStatus::Internal, QStringLiteral("cannot send response headers"), true);
                return;
            }
        }
        uploading = new QBuffer(socket);
        uploading->setData(std::exchange(sendQueue, QByteArray()));
        uploading->open(QIODevice::ReadOnly);
        uploadedBytes = 0;
        stream->sendDATA(uploading, false);
        return;
    }
    if (bFinishRequested)
    {
        SendFinish();
    }
}

void GrpcCall::SendFinish()
{
    bFinishRequested = false;
    if (bClient)
    {
        if (bHalfClosedLocal)
        {
            return;
        }
        bHalfClosedLocal = true;
        auto* empty = new QBuffer(socket);
        empty->open(QIODevice::ReadOnly);
        uploading = empty;
        timeout->start(StatusTimeoutMs);
        stream->sendDATA(empty, true);
        return;
    }
    const QByteArray code = QByteArray::number(finishStatus);
    const QByteArray message = EncodeGrpcMessage(finishMessage);
    const HPack::HttpHeader trailers = bHeadersSent
        ? Fields({{":status", "200"}, {"grpc-status", code}, {"grpc-message", message}})
        : Fields({{":status", "200"}, {"content-type", "application/grpc"}, {"grpc-status", code}, {"grpc-message", message}});
    bHeadersSent = true;
    const bool bSent = stream->sendHEADERS(trailers, true);
    EndCall(finishStatus, finishMessage, !bSent);
}

void GrpcCall::EndCall(int status, const QString& message, bool bReset)
{
    if (bEnded)
    {
        return;
    }
    bEnded = true;
    timeout->stop();
    sendQueue.clear();

    if (socket)
    {
        QTcpSocket* lingering = socket;
        QPointer<QHttp2Connection> lingeringConnection = connection;
        QPointer<QHttp2Stream> lingeringStream = stream;
        disconnect(lingering, nullptr, this, nullptr);
        if (connection)
        {
            disconnect(connection, nullptr, this, nullptr);
        }
        if (stream)
        {
            disconnect(stream, nullptr, this, nullptr);
        }
        socket = nullptr;
        connection = nullptr;
        stream = nullptr;
        uploading = nullptr;
        lingering->setParent(nullptr);
        QTimer::singleShot(0, lingering, [lingering, lingeringConnection, lingeringStream, bReset]
        {
            const bool bConnected = lingering->state() == QAbstractSocket::ConnectedState;
            if (bConnected && bReset && lingeringStream && lingeringStream->isActive())
            {
                lingeringStream->sendRST_STREAM(Http2::CANCEL);
            }
            if (bConnected && lingeringConnection)
            {
                lingeringConnection->close();
            }
            if (lingering->state() == QAbstractSocket::UnconnectedState)
            {
                lingering->deleteLater();
                return;
            }
            QObject::connect(lingering, &QAbstractSocket::disconnected, lingering, &QObject::deleteLater);
            lingering->disconnectFromHost();
            QTimer::singleShot(LingerMs, lingering, [lingering]
            {
                lingering->abort();
                lingering->deleteLater();
            });
        });
    }

    emit finished(status, message);
}

GrpcServer::GrpcServer(const QByteArray& path, QObject* parent)
    : QObject(parent)
    , server(new QTcpServer(this))
    , path(path)
{
    connect(server, &QTcpServer::newConnection, this, &GrpcServer::OnNewConnection);
}

bool GrpcServer::Listen(quint16 port)
{
    if (server->isListening())
    {
        server->close();
    }
    return server->listen(QHostAddress::Any, port);
}

void GrpcServer::Close()
{
    server->close();
}

bool GrpcServer::IsListening() const
{
    return server->isListening();
}

quint16 GrpcServer::Port() const
{
    return server->serverPort();
}

QString GrpcServer::ErrorString() const
{
    return server->errorString();
}

void GrpcServer::OnNewConnection()
{
    while (QTcpSocket* socket = server->nextPendingConnection())
    {
        ConfigureSocket(socket);
        auto* call = new GrpcCall(socket, false, this);
        call->path = path;
        call->peerHost = CleanHost(socket->peerAddress());
        call->peerPort = socket->peerPort();
        call->connection = QHttp2Connection::createDirectServerConnection(socket, Configuration());
        if (!call->connection)
        {
            call->EndCall(GrpcStatus::Unavailable, QStringLiteral("HTTP/2 setup failed"), false);
            call->deleteLater();
            continue;
        }
        connect(socket, &QIODevice::readyRead, call->connection, &QHttp2Connection::handleReadyRead);
        connect(call->connection, &QHttp2Connection::errorOccurred, call, [call](Http2::Http2Error, const QString& message)
        {
            call->EndCall(GrpcStatus::Unavailable, message, false);
        });
        connect(call->connection, &QHttp2Connection::newIncomingStream, call, [call](QHttp2Stream* incoming)
        {
            if (call->stream || call->bEnded)
            {
                connect(incoming, &QHttp2Stream::headersReceived, incoming, [incoming]
                {
                    incoming->sendRST_STREAM(Http2::REFUSE_STREAM);
                });
                return;
            }
            call->AttachStream(incoming);
        });
        connect(call, &GrpcCall::finished, this, [call]
        {
            if (!call->bAccepted)
            {
                call->deleteLater();
            }
        });
        call->timeout->start(RequestTimeoutMs);
    }
}
