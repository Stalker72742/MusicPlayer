//
// Created by Stalker7274 on 27.09.2026.
//

#pragma once

#include <QCryptographicHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

/// @brief GET with the User-Agent header the GitHub API requires.
/// @return The reply; the caller owns it.
QNetworkReply* SendGitHubRequest(QNetworkAccessManager* network, const QUrl& url);

/// @brief Streams a release asset to a file and verifies its SHA-256 against a checksums file.
///
/// The checksums file is either `<sha256>  <asset name>` per line or a PowerShell `Get-FileHash` dump for the
/// single asset. The file appears at the target path only if the hash matches.
/// Emits finished() once, then deletes itself; deleting it earlier aborts the download.
class ReleaseAssetDownload : public QObject
{
    Q_OBJECT

public:
    /// @param network Network manager to use; must outlive the download.
    /// @param assetUrl URL of the asset.
    /// @param checksumsUrl URL of the checksums file.
    /// @param targetPath Where the verified file is written.
    /// @param parent QObject parent.
    ReleaseAssetDownload(QNetworkAccessManager* network, const QUrl& assetUrl, const QUrl& checksumsUrl,
        const QString& targetPath, QObject* parent = nullptr);
    ~ReleaseAssetDownload() override;

    /// @brief Fetches the checksums, then the asset.
    void Start();

signals:
    /// @brief Bytes of the asset received so far.
    void progress(qint64 received, qint64 total);
    /// @brief The download ended; `error` says why when `bSuccess` is false.
    void finished(bool bSuccess, const QString& error);

private:
    void DownloadAsset();
    void Finish(bool bSuccess, const QString& error = {});

    QNetworkAccessManager* network;
    QUrl assetUrl;
    QUrl checksumsUrl;
    QString targetPath;

    QByteArray expectedHash;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    std::unique_ptr<QSaveFile> file;
    QPointer<QNetworkReply> reply;
};
