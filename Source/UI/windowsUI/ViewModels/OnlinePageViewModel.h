#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "Library/Track.h"
#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;
struct OnlineEntry;

/// @brief The screen of one YouTube video, public playlist or channel, opened by link. QML singleton.
///
/// Follows Navigation::onlineUrl, so the back history works; going back loads the page again.
/// The tracks are transient until saved, added to a playlist or downloaded.
class OnlinePageViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(PageState state READ GetState NOTIFY changed)
    Q_PROPERTY(QString error READ GetError NOTIFY changed)
    Q_PROPERTY(QString kind READ GetKind NOTIFY changed)
    Q_PROPERTY(QString url READ GetUrl NOTIFY changed)
    Q_PROPERTY(QString title READ GetTitle NOTIFY changed)
    Q_PROPERTY(QString channel READ GetChannel NOTIFY changed)
    Q_PROPERTY(QString channelUrl READ GetChannelUrl NOTIFY changed)
    /// @brief The playlist the video link came from; empty otherwise.
    Q_PROPERTY(QString playlistUrl READ GetPlaylistUrl NOTIFY changed)
    /// @brief The list is longer than what was loaded.
    Q_PROPERTY(bool limitReached READ IsLimitReached NOTIFY changed)
    Q_PROPERTY(TrackListModel* tracks READ GetTracks CONSTANT)

public:
    /// @brief Loading state of the page.
    enum PageState
    {
        Idle,    ///< Nothing opened yet.
        Loading, ///< Tracks are arriving.
        Done,    ///< Everything is there.
        Failed   ///< See error; tracks that did arrive stay.
    };
    Q_ENUM(PageState)

    /// @brief What to do with the tracks of a link once they are known.
    enum LinkAction
    {
        OpenOnly,     ///< Just show the page.
        AddToLibrary, ///< Also save every track.
        Download      ///< Also queue every track for download.
    };
    Q_ENUM(LinkAction)

    /// @brief The instance shared by C++ and QML.
    static OnlinePageViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static OnlinePageViewModel* create(QQmlEngine*, QJSEngine*);

    PageState GetState() const { return state; }
    QString GetError() const { return error; }
    QString GetKind() const { return kind; }
    QString GetUrl() const { return url; }
    QString GetTitle() const { return title; }
    QString GetChannel() const { return channel; }
    QString GetChannelUrl() const { return channelUrl; }
    QString GetPlaylistUrl() const { return playlistUrl; }
    bool IsLimitReached() const { return bLimitReached; }
    TrackListModel* GetTracks() { return &tracksModel; }

    /// @brief What the text links to.
    /// @return "video", "playlist", "channel", or empty if it is not a link the app can open.
    Q_INVOKABLE QString linkKind(const QString& text) const;

    /// @brief Opens the page of a link, cleaned of everything but what it points at.
    /// @param text Pasted or dropped text.
    /// @param action What to do with the tracks once they are known.
    /// @return false if the text is not a link.
    Q_INVOKABLE bool openLink(const QString& text, OnlinePageViewModel::LinkAction action = OpenOnly);

    /// @brief Opens the channel of an online track; tracks saved without one look it up first.
    Q_INVOKABLE void openAuthor(int trackId);
    /// @brief Opens the playlist the video link came from.
    Q_INVOKABLE void openPlaylist();
    /// @brief Opens the channel of this page.
    Q_INVOKABLE void openChannel();
    /// @brief Opens this page in the browser.
    Q_INVOKABLE void openInBrowser();
    /// @brief Loads the page again after a failure.
    Q_INVOKABLE void retry();

    /// @brief Ids of every track on the page.
    Q_INVOKABLE QVariantList allTrackIds() const;
    /// @brief Queues tracks for download.
    /// @return How many were queued.
    Q_INVOKABLE int download(const QVariantList& trackIds);
    /// @brief Saves tracks to the library.
    /// @return How many were not there before.
    Q_INVOKABLE int addToLibrary(const QVariantList& trackIds);
    /// @brief Makes a playlist of the tracks, which saves them, and opens it.
    Q_INVOKABLE void saveAsPlaylist(const QString& name, const QVariantList& trackIds);

    /// @brief Drops transient tracks that neither the player, the search results nor this page use.
    static void DropUnusedTransientTracks();

signals:
    /// @brief The state or the details of the page changed.
    void changed();

private:
    OnlinePageViewModel();

    void Load(const QString& pageUrl);
    /// Channels of "Topic" artists have no Videos tab: then the channel's main page gives its uploads.
    void StartListing(const QString& listUrl, bool bChannel);
    void Reset();
    void AddEntry(const OnlineEntry& entry);
    void Finish(const QString& failure);
    void ApplyPendingAction();
    void Rebuild();

    PageState state = Idle;
    QString error;
    QString kind;
    QString url;
    QString title;
    QString channel;
    QString channelUrl;
    QString playlistUrl;
    bool bLimitReached = false;
    int limit = 0;

    QList<TrackId> ids;
    QSet<TrackId> idSet;
    TrackListModel tracksModel;
    QTimer rebuildTimer;

    LinkAction pendingAction = OpenOnly;
    quint64 generation = 0;
    quint64 listing = 0;
};
