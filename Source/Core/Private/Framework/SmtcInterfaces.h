//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

/// @file
/// @brief Minimal ABI declarations of Windows.Media.SystemMediaTransportControls, which MinGW headers lack.
/// Method order and GUIDs are copied from the Windows SDK's winrt/windows.media.h; the vtables must match exactly.
/// Keep them out of anonymous namespaces: with internal linkage GCC assumes nothing derives from an interface
/// and devirtualizes calls into its pure virtual methods (a jump to nowhere in optimized builds).

#include <windows.h>

#include <eventtoken.h>
#include <inspectable.h>
#include <winstring.h>

namespace Smtc
{
    /// @brief Windows.Media.MediaPlaybackStatus.
    enum PlaybackStatus : int
    {
        Closed = 0,
        Changing = 1,
        Stopped = 2,
        Playing = 3,
        Paused = 4
    };

    /// @brief Windows.Media.SystemMediaTransportControlsButton.
    enum Button : int
    {
        Play = 0,
        Pause = 1,
        Stop = 2,
        Record = 3,
        FastForward = 4,
        Rewind = 5,
        Next = 6,
        Previous = 7,
        ChannelUp = 8,
        ChannelDown = 9
    };

    /// @brief Windows.Media.MediaPlaybackType::Music.
    constexpr int MusicPlaybackType = 1;

    /// {99FA3FF4-1742-42A6-902E-087D41F965EC}
    constexpr GUID IID_ISystemMediaTransportControls{
        0x99fa3ff4, 0x1742, 0x42a6, {0x90, 0x2e, 0x08, 0x7d, 0x41, 0xf9, 0x65, 0xec}};

    /// {B7F47116-A56F-4DC8-9E11-92031F4A87C2}
    constexpr GUID IID_IButtonPressedEventArgs{
        0xb7f47116, 0xa56f, 0x4dc8, {0x9e, 0x11, 0x92, 0x03, 0x1f, 0x4a, 0x87, 0xc2}};

    /// TypedEventHandler<SystemMediaTransportControls, SystemMediaTransportControlsButtonPressedEventArgs>
    /// {0557E996-7B23-5BAE-AA81-EA0D671143A4}
    constexpr GUID IID_IButtonPressedHandler{
        0x0557e996, 0x7b23, 0x5bae, {0xaa, 0x81, 0xea, 0x0d, 0x67, 0x11, 0x43, 0xa4}};

    /// {DDB0472D-C911-4A1F-86D9-DC3D71A95F5A}, from systemmediatransportcontrolsinterop.h
    constexpr GUID IID_ISystemMediaTransportControlsInterop{
        0xddb0472d, 0xc911, 0x4a1f, {0x86, 0xd9, 0xdc, 0x3d, 0x71, 0xa9, 0x5f, 0x5a}};

    struct ISystemMediaTransportControlsInterop : IInspectable
    {
        virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND appWindow, REFIID riid, void** controls) = 0;
    };

    struct IMusicDisplayProperties : IInspectable
    {
        virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_Title(HSTRING value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_AlbumArtist(HSTRING value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_Artist(HSTRING value) = 0;
    };

    struct IDisplayUpdater : IInspectable
    {
        virtual HRESULT STDMETHODCALLTYPE get_Type(int* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_Type(int value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_AppMediaId(HSTRING* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_AppMediaId(HSTRING value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_Thumbnail(IInspectable** value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_Thumbnail(IInspectable* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_MusicProperties(IMusicDisplayProperties** value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_VideoProperties(IInspectable** value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_ImageProperties(IInspectable** value) = 0;
        virtual HRESULT STDMETHODCALLTYPE CopyFromFileAsync(int type, IInspectable* source, IInspectable** operation) = 0;
        virtual HRESULT STDMETHODCALLTYPE ClearAll() = 0;
        virtual HRESULT STDMETHODCALLTYPE Update() = 0;
    };

    struct IButtonPressedEventArgs : IInspectable
    {
        virtual HRESULT STDMETHODCALLTYPE get_Button(Button* value) = 0;
    };

    /// @brief TypedEventHandler for ButtonPressed. Delegates derive from IUnknown, not IInspectable.
    struct IButtonPressedHandler : IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable* sender, IButtonPressedEventArgs* args) = 0;
    };

    struct ISystemMediaTransportControls : IInspectable
    {
        virtual HRESULT STDMETHODCALLTYPE get_PlaybackStatus(PlaybackStatus* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_PlaybackStatus(PlaybackStatus value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_DisplayUpdater(IDisplayUpdater** value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_SoundLevel(int* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsPlayEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsPlayEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsStopEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsStopEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsPauseEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsPauseEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsRecordEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsRecordEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsFastForwardEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsFastForwardEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsRewindEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsRewindEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsPreviousEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsPreviousEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsNextEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsNextEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsChannelUpEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsChannelUpEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_IsChannelDownEnabled(boolean* value) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_IsChannelDownEnabled(boolean value) = 0;
        virtual HRESULT STDMETHODCALLTYPE add_ButtonPressed(IButtonPressedHandler* handler, EventRegistrationToken* token) = 0;
        virtual HRESULT STDMETHODCALLTYPE remove_ButtonPressed(EventRegistrationToken token) = 0;
        virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(IUnknown* handler, EventRegistrationToken* token) = 0;
        virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(EventRegistrationToken token) = 0;
    };
}
