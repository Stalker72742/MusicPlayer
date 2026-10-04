#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <QAppUpdater/ReleaseFinder.h>

class QJSEngine;
class QQmlEngine;

/// @brief App info for About and app updates for Updates. QML singleton.
///
/// Checks the configured release source in-process;
/// installing is done by the updater exe (it has to replace this process's files). The updater runs without
/// a window (--update --report) and streams its progress through AppControlServer; how an install that
/// closed the app ended is shown after the restart (lastResultText).
class AppInfoViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString name READ GetName CONSTANT)
    Q_PROPERTY(QString version READ GetVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ GetQtVersion CONSTANT)

    Q_PROPERTY(UpdateState updateState READ GetUpdateState NOTIFY updateStateChanged)
    Q_PROPERTY(bool updateAvailable READ IsUpdateAvailable NOTIFY updateStateChanged)
    Q_PROPERTY(QString latestVersion READ GetLatestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseNotes READ GetReleaseNotes NOTIFY updateStateChanged)
    Q_PROPERTY(QString errorText READ GetErrorText NOTIFY updateStateChanged)
    Q_PROPERTY(QString sourceText READ GetSourceText NOTIFY updateStateChanged)
    Q_PROPERTY(QString lastCheckedText READ GetLastCheckedText NOTIFY updateStateChanged)

    /// @brief While Installing: what the updater does and how far it is (0..1, -1 when unknown).
    Q_PROPERTY(QString installStage READ GetInstallStage NOTIFY installChanged)
    Q_PROPERTY(qreal installProgress READ GetInstallProgress NOTIFY installChanged)

    /// @brief The outcome of the last install, shown once after it restarted the app.
    Q_PROPERTY(QString lastResultText READ GetLastResultText CONSTANT)
    Q_PROPERTY(bool lastResultOk READ IsLastResultOk CONSTANT)

public:
    /// @brief Where the update check or install is.
    enum UpdateState
    {
        NotChecked,      ///< No check yet.
        Checking,        ///< Looking for a release.
        UpToDate,        ///< This is the latest version.
        UpdateAvailable, ///< A newer release exists.
        CheckFailed,     ///< The check failed; see errorText.
        Installing,      ///< The updater is running; see installStage / installProgress.
        InstallFailed    ///< The updater failed or went silent; see errorText.
    };
    Q_ENUM(UpdateState)

    /// @brief The instance shared by C++ and QML.
    static AppInfoViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static AppInfoViewModel* create(QQmlEngine*, QJSEngine*);

    QString GetName() const;
    QString GetVersion() const;
    QString GetQtVersion() const;

    UpdateState GetUpdateState() const { return updateState; }
    bool IsUpdateAvailable() const { return updateState == UpdateAvailable; }
    QString GetLatestVersion() const { return release.version.toString(); }
    QString GetReleaseNotes() const { return release.notes; }
    QString GetErrorText() const { return errorText; }
    QString GetSourceText() const;
    QString GetLastCheckedText() const;
    QString GetInstallStage() const { return installStage; }
    qreal GetInstallProgress() const { return installProgress; }
    QString GetLastResultText() const { return lastResultText; }
    bool IsLastResultOk() const { return bLastResultOk; }

    /// @brief Looks for a newer release. Uses the source from updater.json, like the updater itself.
    Q_INVOKABLE void checkForUpdates();

    /// @brief Starts the updater without a window: it downloads, closes this app, installs and restarts it.
    Q_INVOKABLE void installUpdate();
    /// @brief Opens the updater window.
    Q_INVOKABLE void openUpdater();

    /// @brief Installs a deployed folder or a package zip, even if it is not newer (the user picked it).
    Q_INVOKABLE void installFromPath(const QUrl& path);

signals:
    void updateStateChanged();
    void installChanged();
    /// @brief A check or install failed.
    void errorOccurred(const QString& message);

private:
    AppInfoViewModel();

    void SetUpdateState(UpdateState state);
    void StartUpdater(const QStringList& arguments);
    void StartInstall(const QStringList& arguments);
    void FailInstall(const QString& message);

    QAppUpdater::ReleaseFinder finder;
    QAppUpdater::ReleaseInfo release;
    UpdateState updateState = NotChecked;
    QString errorText;
    QDateTime lastChecked;

    QString installStage;
    qreal installProgress = -1;
    /// No word from the updater for this long: it did not start, or is an old one without --report.
    QTimer installWatchdog;

    QString lastResultText;
    bool bLastResultOk = false;
};
