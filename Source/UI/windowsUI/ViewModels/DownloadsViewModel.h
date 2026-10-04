#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// @brief Rows of the download queue.
class DownloadListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DownloadsViewModel")

    Q_PROPERTY(int count READ GetCount NOTIFY countChanged)

public:
    /// @brief Mirrors DownloadSubsystem::Job::State.
    enum State
    {
        Queued,
        Downloading,
        Done,
        Failed
    };
    Q_ENUM(State)

    /// @brief A job as a row shows it.
    struct Row
    {
        int trackId = 0;
        QString title;
        QString artist;
        QString artUrl;
        QColor artTint;
        State state = Queued;
        qreal progress = 0; ///< 0..1; -1 while the size is unknown.
        QString sizeText;
        QString error;
    };

    /// @brief Model roles.
    enum Roles
    {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        ArtUrlRole,
        ArtTintRole,
        StateRole,
        ProgressRole,
        SizeTextRole,
        ErrorRole
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief Number of rows (the count property).
    int GetCount() const { return static_cast<int>(rows.size()); }

    /// @brief Replaces the rows. Same jobs in the same order: updated in place (progress ticks often).
    void SetRows(const QList<Row>& newRows);

signals:
    void countChanged();

private:
    QList<Row> rows;
};

/// @brief The download queue for the UI, backed by DownloadSubsystem. QML singleton.
class DownloadsViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(DownloadListModel* jobs READ GetJobs CONSTANT)
    Q_PROPERTY(int activeCount READ GetActiveCount NOTIFY changed)
    Q_PROPERTY(int finishedCount READ GetFinishedCount NOTIFY changed)
    Q_PROPERTY(QString folder READ GetFolder NOTIFY changed)

public:
    /// @brief The instance shared by C++ and QML.
    static DownloadsViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static DownloadsViewModel* create(QQmlEngine*, QJSEngine*);

    DownloadListModel* GetJobs() { return &jobsModel; }
    int GetActiveCount() const;
    int GetFinishedCount() const;
    QString GetFolder() const;

    /// @brief Queues one track; see DownloadSubsystem::Enqueue().
    Q_INVOKABLE void download(int trackId);
    /// @brief Queues every online track of the playlist that has no local copy yet.
    /// @return How many were queued.
    Q_INVOKABLE int downloadPlaylist(const QString& playlistId);
    /// @brief See DownloadSubsystem::Cancel().
    Q_INVOKABLE void cancel(int trackId);
    /// @brief See DownloadSubsystem::Retry().
    Q_INVOKABLE void retry(int trackId);
    /// @brief See DownloadSubsystem::ClearFinished().
    Q_INVOKABLE void clearFinished();
    /// @brief See DownloadSubsystem::RemoveDownload().
    Q_INVOKABLE void removeDownload(int trackId);
    /// @brief Opens the download folder in the file manager.
    Q_INVOKABLE void openFolder();

    /// @brief For the track menu: -1 without a job, else DownloadListModel::State.
    Q_INVOKABLE int stateOf(int trackId) const;
    /// @brief For the track menu: progress 0..1, or -1 while unknown.
    Q_INVOKABLE qreal progressOf(int trackId) const;

signals:
    /// @brief Jobs or their progress changed.
    void changed();

private:
    DownloadsViewModel();

    void Rebuild();

    DownloadListModel jobsModel;
};
