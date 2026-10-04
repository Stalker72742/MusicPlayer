//
// Created by Stalker7274 on 03.10.2026.
//

#include "SystemMediaControls.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPointer>
#include <QWindow>

#include "Library/LibrarySubsystem.h"
#include "Player/PlayerSubsystem.h"

#ifdef Q_OS_WIN

#include <roapi.h>

#include <atomic>
#include <functional>

#include "SmtcInterfaces.h"

namespace
{
    constexpr GUID IID_AgileObject{0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};

    class ScopedHString
    {
    public:
        explicit ScopedHString(const QString& text)
        {
            WindowsCreateString(reinterpret_cast<LPCWSTR>(text.utf16()), static_cast<UINT32>(text.size()), &value);
        }
        ~ScopedHString()
        {
            if (value)
                WindowsDeleteString(value);
        }
        ScopedHString(const ScopedHString&) = delete;
        ScopedHString& operator=(const ScopedHString&) = delete;

        operator HSTRING() const { return value; }

    private:
        HSTRING value = nullptr;
    };

    class ButtonHandler final : public Smtc::IButtonPressedHandler
    {
    public:
        explicit ButtonHandler(std::function<void(int)> onButton)
            : onButton(std::move(onButton))
        {
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
        {
            if (riid == IID_IUnknown || riid == IID_AgileObject || riid == Smtc::IID_IButtonPressedHandler)
            {
                *object = this;
                AddRef();
                return S_OK;
            }
            *object = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG count = --refCount;
            if (count == 0)
                delete this;
            return count;
        }

        HRESULT STDMETHODCALLTYPE Invoke(IInspectable*, Smtc::IButtonPressedEventArgs* args) override
        {
            Smtc::Button button;
            if (args && SUCCEEDED(args->get_Button(&button)))
                onButton(button);
            return S_OK;
        }

    private:
        std::atomic<ULONG> refCount{1};
        std::function<void(int)> onButton;
    };
}

struct SystemMediaControls::Impl
{
    Smtc::ISystemMediaTransportControls* controls = nullptr;
    ButtonHandler* handler = nullptr;
    EventRegistrationToken token{};
    bool bRoInitialized = false;
    bool bEnabled = false;
};

#else

struct SystemMediaControls::Impl
{
};

#endif

SystemMediaControls::SystemMediaControls(QWindow* window, QObject* parent)
    : QObject(parent)
    , impl(std::make_unique<Impl>())
{
#ifdef Q_OS_WIN
    const HRESULT initResult = RoInitialize(RO_INIT_SINGLETHREADED);
    impl->bRoInitialized = SUCCEEDED(initResult);

    Smtc::ISystemMediaTransportControlsInterop* interop = nullptr;
    const ScopedHString className(QStringLiteral("Windows.Media.SystemMediaTransportControls"));
    HRESULT hr = RoGetActivationFactory(className, Smtc::IID_ISystemMediaTransportControlsInterop, reinterpret_cast<void**>(&interop));

    if (SUCCEEDED(hr))
    {
        hr = interop->GetForWindow(reinterpret_cast<HWND>(window->winId()), Smtc::IID_ISystemMediaTransportControls,
            reinterpret_cast<void**>(&impl->controls));
        interop->Release();
    }

    if (FAILED(hr) || !impl->controls)
    {
        qWarning() << "[MediaControls] System media controls are unavailable, HRESULT" << Qt::hex << hr;
        impl->controls = nullptr;
        return;
    }

    Smtc::ISystemMediaTransportControls* controls = impl->controls;
    controls->put_IsPlayEnabled(true);
    controls->put_IsPauseEnabled(true);
    controls->put_IsStopEnabled(true);
    controls->put_IsNextEnabled(true);
    controls->put_IsPreviousEnabled(true);

    controls->put_IsEnabled(false);
    controls->put_PlaybackStatus(Smtc::Closed);

    QPointer<SystemMediaControls> self(this);
    impl->handler = new ButtonHandler([self](int button) {
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, button] {
            if (self)
                self->OnButtonPressed(button);
        }, Qt::QueuedConnection);
    });
    controls->add_ButtonPressed(impl->handler, &impl->token);
#else
    Q_UNUSED(window);
#endif

    PlayerSubsystem& player = PlayerSubsystem::Get();
    connect(&player, &PlayerSubsystem::currentTrackChanged, this, &SystemMediaControls::UpdateDisplay);
    connect(&player, &PlayerSubsystem::currentTrackChanged, this, &SystemMediaControls::UpdatePlaybackStatus);
    connect(&player, &PlayerSubsystem::playingChanged, this, &SystemMediaControls::UpdatePlaybackStatus);

    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, &SystemMediaControls::UpdateDisplay);
}

SystemMediaControls::~SystemMediaControls()
{
#ifdef Q_OS_WIN
    if (impl->controls)
    {
        impl->controls->remove_ButtonPressed(impl->token);
        impl->controls->put_IsEnabled(false);
        impl->controls->Release();
    }
    if (impl->handler)
        impl->handler->Release();
    if (impl->bRoInitialized)
        RoUninitialize();
#endif
}

bool SystemMediaControls::IsAvailable() const
{
#ifdef Q_OS_WIN
    return impl->controls != nullptr;
#else
    return false;
#endif
}

void SystemMediaControls::OnButtonPressed(int button)
{
#ifdef Q_OS_WIN
    PlayerSubsystem& player = PlayerSubsystem::Get();
    switch (button)
    {
        case Smtc::Play:     player.Play(); break;
        case Smtc::Pause:
        case Smtc::Stop:     player.Pause(); break;
        case Smtc::Next:     player.Next(); break;
        case Smtc::Previous: player.Previous(); break;
        default:             break;
    }
#else
    Q_UNUSED(button);
#endif
}

void SystemMediaControls::UpdatePlaybackStatus()
{
#ifdef Q_OS_WIN
    if (!impl->controls)
        return;

    const PlayerSubsystem& player = PlayerSubsystem::Get();
    if (!player.GetCurrentTrack().IsValid())
        return;

    if (!impl->bEnabled)
    {
        impl->controls->put_IsEnabled(true);
        impl->bEnabled = true;
    }

    impl->controls->put_PlaybackStatus(player.IsPlaying() ? Smtc::Playing : Smtc::Paused);
#endif
}

void SystemMediaControls::UpdateDisplay()
{
#ifdef Q_OS_WIN
    if (!impl->controls)
        return;

    const Track* track = LibrarySubsystem::Get().FindTrack(PlayerSubsystem::Get().GetCurrentTrack());
    if (!track)
        return;

    Smtc::IDisplayUpdater* updater = nullptr;
    if (FAILED(impl->controls->get_DisplayUpdater(&updater)) || !updater)
        return;

    Smtc::IMusicDisplayProperties* music = nullptr;
    updater->put_Type(Smtc::MusicPlaybackType);
    if (SUCCEEDED(updater->get_MusicProperties(&music)) && music)
    {
        music->put_Title(ScopedHString(track->tags.title));
        music->put_Artist(ScopedHString(track->tags.artist));
        music->Release();
    }
    updater->Update();
    updater->Release();
#endif
}
