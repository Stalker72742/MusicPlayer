#include "PlayerViewModel.h"

#include <QJSEngine>

#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "Player/PlayerSubsystem.h"

PlayerViewModel::PlayerViewModel()
{
    PlayerSubsystem& player = PlayerSubsystem::Get();

    connect(&player, &PlayerSubsystem::currentTrackChanged, this, &PlayerViewModel::RefreshTrack);
    connect(&player, &PlayerSubsystem::durationChanged, this, &PlayerViewModel::durationChanged);
    connect(&player, &PlayerSubsystem::durationChanged, this, &PlayerViewModel::positionChanged);
    connect(&player, &PlayerSubsystem::playingChanged, this, &PlayerViewModel::playingChanged);
    connect(&player, &PlayerSubsystem::positionChanged, this, &PlayerViewModel::positionChanged);
    connect(&player, &PlayerSubsystem::volumeChanged, this, &PlayerViewModel::volumeChanged);
    connect(&player, &PlayerSubsystem::shuffleChanged, this, &PlayerViewModel::shuffleChanged);
    connect(&player, &PlayerSubsystem::repeatChanged, this, &PlayerViewModel::repeatChanged);
    connect(&player, &PlayerSubsystem::loadingChanged, this, &PlayerViewModel::loadingChanged);
    connect(&player, &PlayerSubsystem::errorOccurred, this, &PlayerViewModel::SetErrorText);
    connect(&player, &PlayerSubsystem::currentTrackChanged, this, [this] { SetErrorText({}); });

    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, &PlayerViewModel::RefreshTrack);

    RefreshTrack();
}

PlayerViewModel& PlayerViewModel::Get()
{
    static PlayerViewModel instance;
    return instance;
}

PlayerViewModel* PlayerViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

void PlayerViewModel::RefreshTrack()
{
    const Track* track = LibrarySubsystem::Get().FindTrack(PlayerSubsystem::Get().GetCurrentTrack());
    const TrackData data = track ? LibraryViewModel::ToTrackData(*track) : TrackData{};

    if (data.id == trackData.id && data.title == trackData.title && data.artist == trackData.artist
        && data.bLiked == trackData.bLiked && data.artTint == trackData.artTint && data.artUrl == trackData.artUrl)
    {
        return;
    }

    trackData = data;
    emit trackChanged();
}

QString PlayerViewModel::GetTotalText() const
{
    return TrackListModel::FormatDuration(static_cast<int>(PlayerSubsystem::Get().GetDuration() / 1000));
}

bool PlayerViewModel::IsPlaying() const
{
    return PlayerSubsystem::Get().IsPlaying();
}

bool PlayerViewModel::IsLoading() const
{
    return PlayerSubsystem::Get().IsLoading();
}

void PlayerViewModel::SetErrorText(const QString& text)
{
    if (errorText == text)
        return;

    errorText = text;
    emit errorTextChanged();
}

qreal PlayerViewModel::GetProgress() const
{
    const qint64 duration = PlayerSubsystem::Get().GetDuration();
    return duration > 0 ? static_cast<qreal>(PlayerSubsystem::Get().GetPosition()) / duration : 0.0;
}

QString PlayerViewModel::GetElapsedText() const
{
    return TrackListModel::FormatDuration(static_cast<int>(PlayerSubsystem::Get().GetPosition() / 1000));
}

qreal PlayerViewModel::GetVolume() const
{
    return PlayerSubsystem::Get().GetVolume();
}

bool PlayerViewModel::IsShuffle() const
{
    return PlayerSubsystem::Get().IsShuffle();
}

int PlayerViewModel::GetRepeatMode() const
{
    return static_cast<int>(PlayerSubsystem::Get().GetRepeatMode());
}

void PlayerViewModel::play(TrackListModel* list, int row)
{
    if (!list)
        return;

    QList<TrackId> queue;
    for (const TrackData& track : list->GetTracks())
        queue.append(TrackId{static_cast<quint32>(track.id)});

    PlayerSubsystem::Get().PlayQueue(queue, row);
}

void PlayerViewModel::playSingle(int trackId)
{
    const TrackId id{static_cast<quint32>(trackId)};
    PlayerSubsystem& player = PlayerSubsystem::Get();

    if (player.GetCurrentTrack() == id)
        player.Play();
    else if (LibrarySubsystem::Get().FindTrack(id))
        player.PlayQueue({id}, 0);
}

void PlayerViewModel::playFromLibrary(int trackId)
{
    TrackListModel* library = LibraryViewModel::Get().GetTracks();
    const int row = library->RowOf(trackId);
    if (row >= 0)
        play(library, row);
    else
        playSingle(trackId);
}

void PlayerViewModel::togglePlay()
{
    PlayerSubsystem::Get().TogglePlay();
}

void PlayerViewModel::next()
{
    PlayerSubsystem::Get().Next();
}

void PlayerViewModel::previous()
{
    PlayerSubsystem::Get().Previous();
}

void PlayerViewModel::seek(qreal fraction)
{
    const qint64 duration = PlayerSubsystem::Get().GetDuration();
    PlayerSubsystem::Get().Seek(static_cast<qint64>(qBound(0.0, fraction, 1.0) * duration));
}

void PlayerViewModel::setVolume(qreal value)
{
    PlayerSubsystem::Get().SetVolume(static_cast<float>(value));
}

void PlayerViewModel::toggleShuffle()
{
    PlayerSubsystem::Get().SetShuffle(!PlayerSubsystem::Get().IsShuffle());
}

void PlayerViewModel::toggleRepeat()
{
    using Mode = PlayerSubsystem::RepeatMode;
    PlayerSubsystem& player = PlayerSubsystem::Get();
    const Mode next = player.GetRepeatMode() == Mode::Off ? Mode::All
                    : player.GetRepeatMode() == Mode::All ? Mode::One
                                                          : Mode::Off;
    player.SetRepeatMode(next);
}

void PlayerViewModel::toggleLike()
{
    if (HasTrack())
        LibraryViewModel::Get().toggleLiked(trackData.id);
}
