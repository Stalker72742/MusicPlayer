//
// Created by Stalker7274 on 27.09.2026.
//

#include "ReleaseAssetDownload.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>

QNetworkReply* SendGitHubRequest(QNetworkAccessManager* network, const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SoundLink"));
    return network->get(request);
}

ReleaseAssetDownload::ReleaseAssetDownload(QNetworkAccessManager* network, const QUrl& assetUrl,
    const QUrl& checksumsUrl, const QString& targetPath, QObject* parent)
    : QObject(parent)
    , network(network)
    , assetUrl(assetUrl)
    , checksumsUrl(checksumsUrl)
    , targetPath(targetPath)
{
}

ReleaseAssetDownload::~ReleaseAssetDownload()
{
    if (reply)
    {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void ReleaseAssetDownload::Start()
{
    reply = SendGitHubRequest(network, checksumsUrl);

    connect(reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply* checksumsReply = reply;
        checksumsReply->deleteLater();

        if (checksumsReply->error() != QNetworkReply::NoError)
        {
            Finish(false, QStringLiteral("Failed to download checksums: ") + checksumsReply->errorString());
            return;
        }

        const QByteArray assetName = assetUrl.fileName().toUtf8();
        const QByteArray checksums = checksumsReply->readAll();
        for (const QByteArray& line : checksums.split('\n'))
        {
            const QList<QByteArray> fields = line.simplified().split(' ');
            if (fields.size() == 2 && fields[1] == assetName)
            {
                expectedHash = fields[0].toLower();
                break;
            }
        }

        if (expectedHash.isEmpty() && checksums.contains(assetName))
        {
            for (const QByteArray& line : checksums.split('\n'))
            {
                const QList<QByteArray> fields = line.simplified().split(' ');
                if (fields.size() == 3 && fields[0] == "Hash" && fields[1] == ":" && fields[2].size() == 64)
                {
                    expectedHash = fields[2].toLower();
                    break;
                }
            }
        }

        if (expectedHash.isEmpty())
        {
            Finish(false, QStringLiteral("No checksum for ") + assetUrl.fileName());
            return;
        }

        DownloadAsset();
    });
}

void ReleaseAssetDownload::DownloadAsset()
{
    QDir().mkpath(QFileInfo(targetPath).absolutePath());

    file = std::make_unique<QSaveFile>(targetPath);
    if (!file->open(QIODevice::WriteOnly))
    {
        Finish(false, QStringLiteral("Cannot write %1: %2").arg(targetPath, file->errorString()));
        return;
    }

    reply = SendGitHubRequest(network, assetUrl);

    connect(reply, &QNetworkReply::downloadProgress, this, &ReleaseAssetDownload::progress);

    connect(reply, &QNetworkReply::readyRead, this, [this] {
        const QByteArray chunk = reply->readAll();
        hash.addData(chunk);
        file->write(chunk);
    });

    connect(reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply* assetReply = reply;
        assetReply->deleteLater();

        if (assetReply->error() != QNetworkReply::NoError)
        {
            Finish(false, QStringLiteral("Failed to download %1: %2").arg(assetUrl.fileName(), assetReply->errorString()));
            return;
        }

        const QByteArray tail = assetReply->readAll();
        hash.addData(tail);
        file->write(tail);

        if (hash.result().toHex() != expectedHash)
        {
            Finish(false, QStringLiteral("Checksum mismatch for ") + assetUrl.fileName());
            return;
        }

        if (!file->commit())
        {
            Finish(false, QStringLiteral("Cannot write %1: %2").arg(targetPath, file->errorString()));
            return;
        }

        Finish(true);
    });
}

void ReleaseAssetDownload::Finish(bool bSuccess, const QString& error)
{
    file.reset();
    reply = nullptr;

    emit finished(bSuccess, error);
    deleteLater();
}
