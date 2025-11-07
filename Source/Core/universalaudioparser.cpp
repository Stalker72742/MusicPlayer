// universalaudioparser.cpp
#include "universalaudioparser.h"
#include <QByteArray>
#include <QMediaPlayer>

UniversalAudioParser::UniversalAudioParser()
    : networkManager(std::make_unique<QNetworkAccessManager>())
{
    initPatterns();

    onUrlsFound([](const QStringList &urls) {
        qDebug() << "Found URL'ы:";
        for (const QString &url : urls) {
            qDebug() << "  -" << url;
        }
    });

    onError([](const QString &error) {
        qDebug() << "error:" << error;
    });

    onProgress([](const QString &status) {
        qDebug() << "Status:" << status;
    });

    parseUrl(QUrl("https://www.youtube.com/watch?v=lY0NI1bbANU&list=RDlY0NI1bbANU"));

    QMediaPlayer player;

    player.setSource(QUrl("https://rr2---sn-5hneknes.googlevideo.com/initplayback?source=youtube\\u0026oeis=1\\u0026c=WEB\\u0026oad=3200\\u0026ovd=3200\\u0026oaad=11000\\u0026oavd=11000\\u0026ocs=700\\u0026oewis=1\\u0026oputc=1\\u0026ofpcc=1\\u0026msp=1\\u0026odepv=1\\u0026oreouc=1\\u0026id=cbd623b0eecc82c5\\u0026ip=77.246.111.231\\u0026initcwndbps=2551250\\u0026mt=1762305957\\u0026oweuc=\\u0026pxtags=Cg4KAnR4Egg1MTYzNDI1MA\\u0026rxtags=Cg4KAnR4Egg1MTYzMTI2NA%2CCg4KAnR4Egg1MTYzNDI0OQ%2CCg4KAnR4Egg1MTYzNDI1MA%2CCg4KAnR4Egg1MTYzNDI3OA%2CCg4KAnR4Egg1MTYzNDI3OQ%2CCg4KAnR4Egg1MTYzNDI4MA%2CCg4KAnR4Egg1MTYzNDI4MQ")
                     );

    player.play();
}

UniversalAudioParser::~UniversalAudioParser() = default;

void UniversalAudioParser::initPatterns() {
    // Audio URL patterns
    audioPatterns = {
        // Direct audio links
        R"(["']([^"']+\.(?:m3u8|mp3|m4a|aac|ogg|wav|flac))["'])",

        // JSON fields with URLs
        R"(["'](?:audio|stream|media)[Uu]rl["']\s*:\s*["']([^"']+)["'])",
        R"(["'](?:src|source|file|url)["']\s*:\s*["']([^"']+)["'])",

        // HTML5 audio elements
        R"(<audio[^>]*src\s*=\s*["']([^"']+)["'])",
        R"(<source[^>]*src\s*=\s*["']([^"']+)["'][^>]*type\s*=\s*["']audio)",

        // JavaScript Audio objects
        R"(new\s+Audio\s*\(\s*["']([^"']+)["']\s*\))",
        R"(audio\.src\s*=\s*["']([^"']+)["'])",

        // HLS/DASH manifests
        R"(["']([^"']+\.(?:m3u8|mpd))["'])",

        // Platform specific
        R"(["'](?:hls|dash|progressive)[^"']*["']\s*:\s*["']([^"']+)["'])",

        // Base64 data URLs
        R"(["'](data:audio\/[^;]+;base64,[^"']+)["'])"
    };

    jsObjectPatterns = {
        // JS objects with media data
        R"(var\s+\w+\s*=\s*\{[^}]*["']url["']\s*:\s*["']([^"']+)[^}]*\})",
        R"(window\.\w+\s*=\s*\{[^}]*["'](?:audio|media)["'][^}]*["']([^"']+)[^}]*\})",

        // Player configs
        R"(player(?:Config|Options)\s*=\s*\{[^}]*["'](?:file|source)["']\s*:\s*["']([^"']+))",
        R"(jwplayer\([^)]+\)\.setup\(\{[^}]*["']file["']\s*:\s*["']([^"']+))",
        R"(videojs\([^)]+\)\s*\.src\(\{[^}]*["']src["']\s*:\s*["']([^"']+))"
    };
}

void UniversalAudioParser::parseUrl(const QUrl &url) {
    emitProgress("Loading page...");

    QNetworkRequest request(url);

    // Set User-Agent to avoid blocks
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                         "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");

    QNetworkReply *reply = networkManager->get(request);

    // Handle SSL errors
    QObject::connect(reply, &QNetworkReply::sslErrors,
                     [reply](const QList<QSslError> &errors) {
                         qDebug() << "SSL errors encountered:";
                         for (const auto &error : errors) {
                             qDebug() << "  -" << error.errorString();
                         }
                         reply->ignoreSslErrors(); // Ignore SSL errors for now
                     });

    // Handle response
    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError &&
            reply->error() != QNetworkReply::SslHandshakeFailedError) {
            emitError(QString("Network error: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        processResponse(reply);
        reply->deleteLater();
    });
}

void UniversalAudioParser::processResponse(QNetworkReply *reply) {
    emitProgress("Analyzing content...");

    QByteArray data = reply->readAll();
    QString content = QString::fromUtf8(data);

    // Search for URLs
    QStringList foundUrls = findAudioUrls(content);

    // Normalize URLs relative to base URL
    QUrl baseUrl = reply->url();
    QStringList normalizedUrls;

    for (const QString &url : foundUrls) {
        QString normalized = normalizeUrl(url, baseUrl);
        if (!normalized.isEmpty() && !normalizedUrls.contains(normalized)) {
            normalizedUrls.append(normalized);
        }
    }

    // If nothing found, try base64 encoded content
    if (normalizedUrls.isEmpty()) {
        emitProgress("Searching in encoded scripts...");

        QRegularExpression base64Regex(R"(atob\(["']([^"']+)["']\))");
        auto matches = base64Regex.globalMatch(content);

        while (matches.hasNext()) {
            auto match = matches.next();
            QString base64Data = match.captured(1);
            QByteArray decoded = QByteArray::fromBase64(base64Data.toUtf8());
            QString decodedStr = QString::fromUtf8(decoded);

            QStringList decodedUrls = findAudioUrls(decodedStr);
            for (const QString &url : decodedUrls) {
                QString normalized = normalizeUrl(url, baseUrl);
                if (!normalized.isEmpty() && !normalizedUrls.contains(normalized)) {
                    normalizedUrls.append(normalized);
                }
            }
        }
    }

    if (!normalizedUrls.isEmpty()) {
        emitProgress(QString("Found %1 audio URL(s)").arg(normalizedUrls.size()));
        emitUrlsFound(normalizedUrls);
    } else {
        emitError("Could not find any audio URLs on the page");
    }
}

QStringList UniversalAudioParser::findAudioUrls(const QString &content) {
    QStringList urls;

    // Apply main patterns
    for (const QString &pattern : audioPatterns) {
        QRegularExpression rx(pattern, QRegularExpression::CaseInsensitiveOption);
        auto matches = rx.globalMatch(content);

        while (matches.hasNext()) {
            auto match = matches.next();
            QString url = match.captured(1);

            if (isValidAudioUrl(url)) {
                urls.append(url);
                qDebug() << "Found audio URL:" << url;
            }
        }
    }

    // Apply JS patterns
    for (const QString &pattern : jsObjectPatterns) {
        QRegularExpression rx(pattern, QRegularExpression::DotMatchesEverythingOption);
        auto matches = rx.globalMatch(content);

        while (matches.hasNext()) {
            auto match = matches.next();
            QString url = match.captured(1);

            if (isValidAudioUrl(url)) {
                urls.append(url);
                qDebug() << "Found JS audio URL:" << url;
            }
        }
    }

    // Parse JSON objects
    QRegularExpression jsonRx(R"(\{[^{}]*["'](?:url|src|source|file)["'][^{}]*\})");
    auto jsonMatches = jsonRx.globalMatch(content);

    while (jsonMatches.hasNext()) {
        auto match = jsonMatches.next();
        QString jsonStr = match.captured(0);

        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();

            QStringList keys = {"url", "src", "source", "file", "audio", "audioUrl",
                                "streamUrl", "mediaUrl", "hls", "dash", "progressive"};

            for (const QString &key : keys) {
                if (obj.contains(key)) {
                    QString url = obj[key].toString();
                    if (isValidAudioUrl(url)) {
                        urls.append(url);
                        qDebug() << "Found JSON audio URL:" << url;
                    }
                }
            }
        }
    }

    qDebug() << "Total URLs found:" << urls.size();
    return urls;
}

QString UniversalAudioParser::normalizeUrl(const QString &url, const QUrl &baseUrl) {
    if (url.isEmpty()) {
        return "";
    }

    // Data URL - return as is
    if (url.startsWith("data:")) {
        return url;
    }

    // Absolute URL
    if (url.startsWith("http://") || url.startsWith("https://")) {
        return url;
    }

    // Protocol-relative URL
    if (url.startsWith("//")) {
        return baseUrl.scheme() + ":" + url;
    }

    // Root-relative URL
    if (url.startsWith("/")) {
        return baseUrl.scheme() + "://" + baseUrl.host() + url;
    }

    // Relative to current directory
    QString basePath = baseUrl.path();
    int lastSlash = basePath.lastIndexOf('/');
    if (lastSlash != -1) {
        basePath = basePath.left(lastSlash + 1);
    } else {
        basePath = "/";
    }

    return baseUrl.scheme() + "://" + baseUrl.host() + basePath + url;
}

QString UniversalAudioParser::decodeUrl(const QString &encodedUrl) {
    QString decoded = encodedUrl;

    // Decode Unicode escapes
    decoded.replace("\\u0026", "&");
    decoded.replace("\\u003d", "=");
    decoded.replace("\\u002f", "/");

    // URL decode
    decoded = QUrl::fromPercentEncoding(decoded.toUtf8());

    return decoded;
}

bool UniversalAudioParser::isValidAudioUrl(const QString &url) {
    // Filter out invalid URLs
    static const QStringList blacklist = {
        "youtube.com/@",      // Channel URLs
        "ytimg.com",          // YouTube images
        ".jpg", ".jpeg",      // Images
        ".png", ".gif",       // Images
        ".webp", ".svg",      // Images
        "accounts.google",    // Login pages
        "ServiceLogin",       // Auth pages
        "/channel/",          // YouTube channels
        "/user/",             // YouTube users
        "/playlist"           // Playlists
    };

    for (const QString &blocked : blacklist) {
        if (url.contains(blocked, Qt::CaseInsensitive)) {
            return false;
        }
    }

    // Only accept specific audio formats for YouTube
    if (url.contains("youtube.com") || url.contains("youtu.be")) {
        // For YouTube, only accept specific patterns
        return url.contains("googlevideo.com") ||
            url.contains("/videoplayback");
    }

    // Rest of validation...
    return true;
}
