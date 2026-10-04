#include "OnlineViewModel.h"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QStandardPaths>
#include <QUrl>

#include "Online/BrowserBridge.h"
#include "Online/OnlineSubsystem.h"
#include "YtDlp/DenoSubsystem.h"
#include "YtDlp/YtDlpSubsystem.h"

OnlineViewModel::OnlineViewModel()
{
    connect(&BrowserBridge::Get(), &BrowserBridge::stateChanged, this, &OnlineViewModel::bridgeChanged);

    connect(&OnlineSubsystem::Get(), &OnlineSubsystem::backendsChanged, this, &OnlineViewModel::backendsChanged);
    connect(&BrowserBridge::Get(), &BrowserBridge::stateChanged, this, &OnlineViewModel::backendsChanged);
    connect(&YtDlpSubsystem::Get(), &YtDlpSubsystem::stateChanged, this, &OnlineViewModel::backendsChanged);
    connect(&DenoSubsystem::Get(), &DenoSubsystem::stateChanged, this, &OnlineViewModel::backendsChanged);
}

OnlineViewModel& OnlineViewModel::Get()
{
    static OnlineViewModel instance;
    return instance;
}

OnlineViewModel* OnlineViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

OnlineViewModel::BridgeState OnlineViewModel::GetBridgeState() const
{
    return static_cast<BridgeState>(BrowserBridge::Get().GetState());
}

QString OnlineViewModel::GetBrowserName() const
{
    return BrowserBridge::Get().GetBrowserName();
}

QString OnlineViewModel::GetPairingCode() const
{
    return BrowserBridge::Get().GetPairingCode();
}

int OnlineViewModel::GetPairedCount() const
{
    return BrowserBridge::Get().GetPairedCount();
}

QVariantList OnlineViewModel::GetBackends() const
{
    QVariantList result;
    for (const OnlineSubsystem::BackendInfo& backend : OnlineSubsystem::Get().GetBackends())
    {
        QStringList roles;
        if (backend.bSearch)
            roles << QStringLiteral("search");
        if (backend.bStream)
            roles << QStringLiteral("streams");

        result.append(QVariantMap{
            {QStringLiteral("name"), backend.name},
            {QStringLiteral("roles"), roles.join(QStringLiteral(", "))},
            {QStringLiteral("status"), backend.status},
        });
    }
    return result;
}

QString OnlineViewModel::GetLastSearchBackend() const
{
    return OnlineSubsystem::Get().GetLastSearchBackend();
}

QString OnlineViewModel::GetLastStreamBackend() const
{
    return OnlineSubsystem::Get().GetLastStreamBackend();
}

void OnlineViewModel::forgetPairedBrowsers()
{
    BrowserBridge::Get().ForgetPairedBrowsers();
}

void OnlineViewModel::openExtensionFolder()
{
    const QString target = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/browser-extension";
    QDir(target).removeRecursively();
    QDir().mkpath(target);

    QDirIterator it(QStringLiteral(":/extension"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString source = it.next();
        const QString destination = target + source.mid(QStringLiteral(":/extension").size());
        QDir().mkpath(QFileInfo(destination).absolutePath());
        QFile::copy(source, destination);
        QFile::setPermissions(destination, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}
