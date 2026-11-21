//
// Created by Stalker7274 on 05.11.2026.
//

#include "Player/AudioDevices.h"

#include <initguid.h>

#include <mmdeviceapi.h>
#include <propidl.h>
#include <propsys.h>
#include <qt_windows.h>

#include <QMetaObject>

#include <atomic>

namespace
{
    const CLSID ClsidEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
    const IID IidEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
    const IID IidNotificationClient = {0x7991EEC9, 0x7E89, 0x4D85, {0x83, 0x90, 0x6C, 0x70, 0x3C, 0xEC, 0x60, 0xC0}};
    const IID IidUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    const PROPERTYKEY KeyFriendlyName = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14};

    IMMDeviceEnumerator* CreateEnumerator()
    {
        IMMDeviceEnumerator* enumerator = nullptr;
        CoCreateInstance(ClsidEnumerator, nullptr, CLSCTX_ALL, IidEnumerator, reinterpret_cast<void**>(&enumerator));
        return enumerator;
    }

    /// COM may not be up on this thread yet; a thread already in another apartment is fine as it is.
    class ComScope
    {
    public:
        ComScope()
            : bOwned(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        {
        }
        ~ComScope()
        {
            if (bOwned)
                CoUninitialize();
        }

    private:
        bool bOwned;
    };

    QByteArray IdOf(IMMDevice* device)
    {
        wchar_t* id = nullptr;
        if (FAILED(device->GetId(&id)))
            return {};
        const QByteArray result = QString::fromWCharArray(id).toUtf8();
        CoTaskMemFree(id);
        return result;
    }
}

class AudioDeviceNotifier : public IMMNotificationClient
{
public:
    explicit AudioDeviceNotifier(AudioDevices* owner)
        : owner(owner)
    {
    }

    /// The system calls back on its own thread: the owner hears about it on its own.
    void Notify()
    {
        QMetaObject::invokeMethod(owner, &AudioDevices::changed, Qt::QueuedConnection);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
    {
        if (iid == IidUnknown || iid == IidNotificationClient)
        {
            *out = this;
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG left = --references;
        if (left == 0)
            delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
    {
        Notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override
    {
        Notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
    {
        Notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override
    {
        if (flow == eRender && role == eMultimedia)
            Notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    IMMDeviceEnumerator* enumerator = nullptr;

private:
    AudioDevices* owner;
    std::atomic<ULONG> references{1};
};

QList<AudioDevices::Device> AudioDevices::List()
{
    const ComScope com;
    QList<Device> devices;

    IMMDeviceEnumerator* enumerator = CreateEnumerator();
    if (!enumerator)
        return devices;

    IMMDeviceCollection* collection = nullptr;
    UINT count = 0;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)))
        collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i)
    {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device)))
            continue;

        Device entry;
        entry.id = IdOf(device);

        IPropertyStore* properties = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties)))
        {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(KeyFriendlyName, &value)) && value.vt == VT_LPWSTR)
                entry.name = QString::fromWCharArray(value.pwszVal);
            PropVariantClear(&value);
            properties->Release();
        }
        device->Release();

        if (entry.name.isEmpty())
            entry.name = QString::fromUtf8(entry.id);
        devices.append(entry);
    }

    if (collection)
        collection->Release();
    enumerator->Release();
    return devices;
}

QByteArray AudioDevices::Resolve(const QByteArray& wanted)
{
    const ComScope com;

    if (!wanted.isEmpty())
    {
        for (const Device& device : List())
        {
            if (device.id == wanted)
                return wanted;
        }
    }

    IMMDeviceEnumerator* enumerator = CreateEnumerator();
    if (!enumerator)
        return {};

    QByteArray id;
    IMMDevice* device = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)))
    {
        id = IdOf(device);
        device->Release();
    }
    enumerator->Release();
    return id;
}

AudioDevices::AudioDevices(QObject* parent)
    : QObject(parent)
    , notifier(new AudioDeviceNotifier(this))
{
    const ComScope com;
    notifier->enumerator = CreateEnumerator();
    if (notifier->enumerator)
        notifier->enumerator->RegisterEndpointNotificationCallback(notifier);
}

AudioDevices::~AudioDevices()
{
    const ComScope com;
    if (notifier->enumerator)
    {
        notifier->enumerator->UnregisterEndpointNotificationCallback(notifier);
        notifier->enumerator->Release();
    }
    notifier->Release();
}
