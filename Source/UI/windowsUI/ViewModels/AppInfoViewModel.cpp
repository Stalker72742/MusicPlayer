#include "AppInfoViewModel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QJSEngine>
#include <QLocale>

#include <QAppUpdater/AppControlServer.h>
#include <QAppUpdater/InstallLayout.h>
#include <QAppUpdater/UpdaterConfig.h>

#include "AppInstance.h"

using namespace QAppUpdater;

namespace
{
    UpdaterConfig LoadUpdaterConfig()
    {
        UpdaterConfig config;
        QString error;
        if (!UpdaterConfig::load(UpdaterConfig::defaultPath(), config, &error))
            qWarning() << "[Updates] Bad" << UpdaterConfig::defaultPath() << ":" << error;
        return config;
    }

    constexpr int InstallSilenceTimeoutMs = 60'000;
}

AppInfoViewModel::AppInfoViewModel()
{
    connect(&finder, &ReleaseFinder::found, this, [this](const ReleaseInfo& found) {
        release = found;
        errorText.clear();
        lastChecked = QDateTime::currentDateTime();

        const bool bNewer = InstallLayout::isNewer(release.version, InstallLayout::installedVersion(InstallLayout::rootDir()));
        SetUpdateState(bNewer ? UpdateAvailable : UpToDate);
    });

    connect(&finder, &ReleaseFinder::failed, this, [this](const QString& error) {
        release = {};
        errorText = error;
        lastChecked = QDateTime::currentDateTime();
        SetUpdateState(CheckFailed);
    });

    installWatchdog.setSingleShot(true);
    installWatchdog.setInterval(InstallSilenceTimeoutMs);
    connect(&installWatchdog, &QTimer::timeout, this,
        [this] { FailInstall(QStringLiteral("The updater stopped responding. Try again, or open the updater.")); });

    if (AppInstance* app = AppInstance::getInstance(); app && app->getControlServer())
    {
        QAppUpdater::AppControlServer* server = app->getControlServer();
        connect(server, &QAppUpdater::AppControlServer::updaterStatus, this,
            [this](const QString& stage, qint64 done, qint64 total) {
                if (updateState != Installing)
                    SetUpdateState(Installing);
                installStage = stage;
                installProgress = total > 0 ? qBound<qreal>(0, static_cast<qreal>(done) / total, 1) : -1;
                installWatchdog.start();
                qDebug() << "[Updates]" << stage << done << "/" << total;
                emit installChanged();
            });
        connect(server, &QAppUpdater::AppControlServer::updaterFinished, this, [this](bool bOk, const QString& message) {
            installWatchdog.stop();
            if (bOk)
            {
                errorText.clear();
                SetUpdateState(UpToDate);
            }
            else
            {
                FailInstall(message);
            }
        });
    }

    if (const auto result = InstallLayout::takeUpdateResult(InstallLayout::rootDir()))
    {
        bLastResultOk = result->ok;
        lastResultText = result->ok ? QStringLiteral("Updated to %1.").arg(result->version.toString())
                                    : QStringLiteral("The update to %1 failed and was rolled back: %2")
                                          .arg(result->version.toString(), result->message);
    }
}

AppInfoViewModel& AppInfoViewModel::Get()
{
    static AppInfoViewModel instance;
    return instance;
}

AppInfoViewModel* AppInfoViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

QString AppInfoViewModel::GetName() const
{
    return QCoreApplication::applicationName();
}

QString AppInfoViewModel::GetVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString AppInfoViewModel::GetQtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppInfoViewModel::GetSourceText() const
{
    return LoadUpdaterConfig().describeSource();
}

QString AppInfoViewModel::GetLastCheckedText() const
{
    return lastChecked.isValid() ? QLocale().toString(lastChecked, QLocale::ShortFormat) : QString();
}

void AppInfoViewModel::checkForUpdates()
{
    if (updateState == Checking)
        return;

    SetUpdateState(Checking);
    finder.find(LoadUpdaterConfig());
}

void AppInfoViewModel::installUpdate()
{
    StartInstall({QStringLiteral("--update"), QStringLiteral("--report")});
}

void AppInfoViewModel::openUpdater()
{
    StartUpdater({});
}

void AppInfoViewModel::installFromPath(const QUrl& path)
{
    if (!path.isLocalFile())
        return;

    StartInstall({QStringLiteral("--path"), QDir::toNativeSeparators(path.toLocalFile()), QStringLiteral("--update"),
        QStringLiteral("--force"), QStringLiteral("--report")});
}

void AppInfoViewModel::StartInstall(const QStringList& arguments)
{
    if (updateState == Installing)
        return;

    errorText.clear();
    installStage = QStringLiteral("Starting the updater...");
    installProgress = -1;
    SetUpdateState(Installing);
    emit installChanged();

    QString error;
    if (!InstallLayout::startUpdater(InstallLayout::rootDir(), arguments, &error))
    {
        FailInstall(QStringLiteral("Cannot start the updater: ") + error);
        return;
    }
    installWatchdog.start();
}

void AppInfoViewModel::FailInstall(const QString& message)
{
    installWatchdog.stop();
    qWarning() << "[Updates] Install failed:" << message;
    errorText = message;
    SetUpdateState(InstallFailed);
}

void AppInfoViewModel::SetUpdateState(UpdateState state)
{
    updateState = state;
    emit updateStateChanged();
}

void AppInfoViewModel::StartUpdater(const QStringList& arguments)
{
    QString error;
    if (!InstallLayout::startUpdater(InstallLayout::rootDir(), arguments, &error))
    {
        qWarning() << "[Updates] Cannot start the updater:" << error;
        emit errorOccurred(QStringLiteral("Cannot start the updater: ") + error);
    }
}
