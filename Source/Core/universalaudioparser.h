// universalaudioparser.h
#ifndef UNIVERSALAUDIOPARSER_H
#define UNIVERSALAUDIOPARSER_H

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <functional>
#include <memory>

class UniversalAudioParser {
public:
    // Типы коллбэков
    using UrlsFoundCallback = std::function<void(const QStringList&)>;
    using ErrorCallback = std::function<void(const QString&)>;
    using ProgressCallback = std::function<void(const QString&)>;

    UniversalAudioParser();
    ~UniversalAudioParser();

    // Основной метод парсинга
    void parseUrl(const QUrl &url);

    // Установка коллбэков
    void onUrlsFound(UrlsFoundCallback cb) { urlsFoundCallback = cb; }
    void onError(ErrorCallback cb) { errorCallback = cb; }
    void onProgress(ProgressCallback cb) { progressCallback = cb; }

private:
    std::unique_ptr<QNetworkAccessManager> networkManager;

    // Коллбэки
    UrlsFoundCallback urlsFoundCallback;
    ErrorCallback errorCallback;
    ProgressCallback progressCallback;

    // Паттерны для поиска
    QStringList audioPatterns;
    QStringList jsObjectPatterns;

    // Вспомогательные методы
    void initPatterns();
    QStringList findAudioUrls(const QString &content);
    QString normalizeUrl(const QString &url, const QUrl &baseUrl);
    bool isValidAudioUrl(const QString &url);
    void processResponse(QNetworkReply *reply);

    // Хелперы для коллбэков
    void emitUrlsFound(const QStringList &urls) {
        if (urlsFoundCallback) urlsFoundCallback(urls);
    }

    void emitError(const QString &error) {
        if (errorCallback) errorCallback(error);
    }

    void emitProgress(const QString &status) {
        if (progressCallback) progressCallback(status);
    }

    QString decodeUrl(const QString &encodedUrl);
};

#endif // UNIVERSALAUDIOPARSER_H
