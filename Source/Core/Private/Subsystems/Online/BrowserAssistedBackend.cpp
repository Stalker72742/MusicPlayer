//
// Created by Stalker7274 on 04.10.2026.
//

#include "BrowserAssistedBackend.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>

#include <QTomlUtils/QTomlUtils.h>

#include "AppConfigs.h"
#include "Online/BrowserBridge.h"
#include "YtDlp/DenoSubsystem.h"
#include "YtDlp/YtDlpSubsystem.h"

namespace
{
    constexpr qint64 MaxTokenAgeSec = 6 * 60 * 60;

    constexpr int ProbeTimeoutMs = 8'000;
}

BrowserAssistedBackend::BrowserAssistedBackend(YtDlpBackend* ytDlp, QObject* parent)
    : OnlineBackend(parent)
    , ytDlp(ytDlp)
{
}

void BrowserAssistedBackend::Shutdown()
{
    delete network;
    network = nullptr;
}

QString BrowserAssistedBackend::GetUnavailableReason() const
{
    if (!BrowserBridge::Get().IsConnected())
        return QStringLiteral("The browser extension is not connected");
    if (!YtDlpSubsystem::Get().IsAvailable())
        return QStringLiteral("yt-dlp is not installed yet");
    if (!DenoSubsystem::Get().IsAvailable())
        return QStringLiteral("The JavaScript runtime for yt-dlp is not installed yet");
    return {};
}

bool BrowserAssistedBackend::AreCookiesAllowed()
{
    return QTomlUtils::FindPropertyValue<QString>(AppConfigs::Settings, AppConfigs::SettingsKeys::OnlineBrowserCookies)
               .value_or(QString())
        != AppConfigs::BrowserCookies::Never;
}

void BrowserAssistedBackend::ResolveStream(const QString& pageUrl, StreamCallback callback)
{
    if (const QString reason = GetUnavailableReason(); !reason.isEmpty())
    {
        callback({}, OnlineError::Make(OnlineError::Unavailable, reason));
        return;
    }

    BrowserBridge::Get().Request(QStringLiteral("credentials"), {}, this,
        [this, pageUrl, callback](const QJsonObject& result, const QString& error) {
            if (!error.isEmpty())
            {
                callback({}, OnlineError::Make(OnlineError::Unavailable, error));
                return;
            }

            const QString poToken = result.value("poToken").toString();
            const qint64 tokenAge = result.value("poTokenAgeSec").toInteger(-1);
            if (poToken.isEmpty() || tokenAge < 0 || tokenAge > MaxTokenAgeSec)
            {
                callback({}, OnlineError::Make(OnlineError::Unavailable,
                    QStringLiteral("No fresh token in the browser yet: play any video on YouTube there")));
                return;
            }

            if (result.value("loggedIn").toBool())
            {
                if (!AreCookiesAllowed())
                {
                    callback({}, OnlineError::Make(OnlineError::Unavailable,
                        QStringLiteral("The browser is logged in to YouTube and sharing cookies is off")));
                    return;
                }
                RunWithCookies(pageUrl, poToken, callback);
                return;
            }

            YtDlpBackend::Credentials credentials;
            credentials.poToken = poToken;
            credentials.visitorData = result.value("visitorData").toString();

            ResolveChecked(pageUrl, credentials,
                [this, pageUrl, poToken, callback](const QUrl& stream, const OnlineError& runError) {
                    if (runError.kind == OnlineError::Blocked && AreCookiesAllowed())
                    {
                        RunWithCookies(pageUrl, poToken, callback);
                        return;
                    }
                    callback(stream, runError);
                });
        });
}

void BrowserAssistedBackend::RunWithCookies(const QString& pageUrl, const QString& poToken, StreamCallback callback)
{
    BrowserBridge::Get().Request(QStringLiteral("cookies"), {}, this,
        [this, pageUrl, poToken, callback](const QJsonObject& result, const QString& error) {
            if (!error.isEmpty())
            {
                callback({}, OnlineError::Make(OnlineError::Unavailable, error));
                return;
            }

            const QString cookiesFile = WriteCookiesFile(result);
            if (cookiesFile.isEmpty())
            {
                callback({}, OnlineError::Make(OnlineError::Failed, QStringLiteral("Cannot write the cookies file")));
                return;
            }

            YtDlpBackend::Credentials credentials;
            credentials.poToken = poToken;
            credentials.cookiesFile = cookiesFile;

            ResolveChecked(pageUrl, credentials, [cookiesFile, callback](const QUrl& stream, const OnlineError& runError) {
                QFile::remove(cookiesFile);
                callback(stream, runError);
            });
        });
}

void BrowserAssistedBackend::ResolveChecked(const QString& pageUrl, const YtDlpBackend::Credentials& credentials,
    StreamCallback callback)
{
    ytDlp->ResolveStreamWith(pageUrl, credentials, [this, callback](const QUrl& stream, const OnlineError& error) {
        if (!error.IsOk())
        {
            callback({}, error);
            return;
        }

        Probe(stream, [stream, callback](const OnlineError& probeError) {
            callback(probeError.IsOk() ? stream : QUrl(), probeError);
        });
    });
}

void BrowserAssistedBackend::Probe(const QUrl& stream, std::function<void(const OnlineError& error)> onDone)
{
    if (!network)
        network = new QNetworkAccessManager(this);

    const qint64 length = QUrlQuery(stream).queryItemValue(QStringLiteral("clen")).toLongLong();
    const qint64 offset = length > 4 * 1024 * 1024 ? length / 2 : 0;

    QNetworkRequest request(stream);
    request.setRawHeader("Range", QStringLiteral("bytes=%1-%2").arg(offset).arg(offset + 1).toLatin1());
    request.setTransferTimeout(ProbeTimeoutMs);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);

    QNetworkReply* reply = network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, onDone] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200 || status == 206)
        {
            onDone({});
            return;
        }
        onDone(OnlineError::Make(OnlineError::Blocked,
            QStringLiteral("The stream refused the browser's token (HTTP %1)").arg(status)));
    });
}

QString BrowserAssistedBackend::WriteCookiesFile(const QJsonObject& result)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tmp";
    QDir().mkpath(dir);
    const QString path = dir + "/cookies-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".txt";

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    QByteArray text = "# Netscape HTTP Cookie File\n";
    for (const QJsonValue& value : result.value("cookies").toArray())
    {
        const QJsonObject cookie = value.toObject();
        const QString domain = cookie.value("domain").toString();
        if (domain.isEmpty() || cookie.value("name").toString().isEmpty())
            continue;

        const bool bSubdomains = !cookie.value("hostOnly").toBool();
        const QString fileDomain = bSubdomains && !domain.startsWith('.') ? QStringLiteral(".") + domain : domain;
        const QStringList fields{
            fileDomain,
            bSubdomains ? QStringLiteral("TRUE") : QStringLiteral("FALSE"),
            cookie.value("path").toString(QStringLiteral("/")),
            cookie.value("secure").toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE"),
            QString::number(static_cast<qint64>(cookie.value("expirationDate").toDouble())),
            cookie.value("name").toString(),
            cookie.value("value").toString(),
        };
        text += fields.join('\t').toUtf8() + '\n';
    }

    file.write(text);
    return path;
}
