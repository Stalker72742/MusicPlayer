//
// Created by Stalker7274 on 05.11.2026.
//

#include "WasapiOutput.h"

#include <initguid.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <qt_windows.h>

#include <QDebug>
#include <QVarLengthArray>

#include <algorithm>

#include "PcmRing.h"

namespace
{
    const CLSID ClsidEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
    const IID IidEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
    const IID IidAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
    const IID IidRenderClient = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};
    const GUID SubtypeFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

    constexpr DWORD FlagEventCallback = 0x00040000;
    constexpr DWORD FlagAutoConvert = 0x80000000;
    constexpr DWORD FlagSrcQuality = 0x08000000;

    constexpr REFERENCE_TIME BufferDuration = 1'000'000;
    constexpr DWORD WaitMs = 100;

    template <typename T>
    struct ComRef
    {
        T* p = nullptr;

        ComRef() = default;
        ComRef(const ComRef&) = delete;
        ComRef& operator=(const ComRef&) = delete;
        ~ComRef()
        {
            if (p)
                p->Release();
        }

        T* operator->() const { return p; }
        void** Out() { return reinterpret_cast<void**>(&p); }
    };

    QString HresultText(const char* what, HRESULT code)
    {
        return QStringLiteral("%1 failed (0x%2)").arg(QLatin1String(what)).arg(static_cast<quint32>(code), 8, 16, QLatin1Char('0'));
    }
}

WasapiOutput::WasapiOutput(QObject* parent)
    : QThread(parent)
    , stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    , commandEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr))
{
}

WasapiOutput::~WasapiOutput()
{
    SetEvent(stopEvent);
    wait();
    CloseHandle(stopEvent);
    CloseHandle(commandEvent);
}

QString WasapiOutput::Open(const QByteArray& deviceId)
{
    requestedId = deviceId;
    start(QThread::TimeCriticalPriority);
    ready.acquire();
    return openError;
}

void WasapiOutput::Start()
{
    bWantRunning = true;
    SetEvent(commandEvent);
}

void WasapiOutput::Pause()
{
    bWantRunning = false;
    SetEvent(commandEvent);
}

void WasapiOutput::run()
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool bComInitialized = SUCCEEDED(comResult);

    HANDLE renderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    {
        const auto failOpen = [this](const QString& message) {
            openError = message;
            ready.release();
        };

        ComRef<IMMDeviceEnumerator> enumerator;
        HRESULT code = CoCreateInstance(ClsidEnumerator, nullptr, CLSCTX_ALL, IidEnumerator, enumerator.Out());
        if (FAILED(code))
        {
            failOpen(HresultText("Creating the audio device list", code));
            CloseHandle(renderEvent);
            if (bComInitialized)
                CoUninitialize();
            return;
        }

        ComRef<IMMDevice> device;
        if (!requestedId.isEmpty())
            code = enumerator->GetDevice(reinterpret_cast<const wchar_t*>(QString::fromUtf8(requestedId).utf16()), &device.p);
        if (requestedId.isEmpty() || FAILED(code))
            code = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device.p);

        ComRef<IAudioClient> client;
        ComRef<IAudioRenderClient> render;
        UINT32 bufferFrames = 0;

        if (SUCCEEDED(code))
        {
            wchar_t* id = nullptr;
            if (SUCCEEDED(device->GetId(&id)))
            {
                endpoint = QString::fromWCharArray(id).toUtf8();
                CoTaskMemFree(id);
            }
            code = device->Activate(IidAudioClient, CLSCTX_ALL, nullptr, client.Out());
        }
        if (FAILED(code))
        {
            failOpen(code == E_NOTFOUND ? QStringLiteral("No audio output device") : HresultText("Opening the audio device", code));
            CloseHandle(renderEvent);
            if (bComInitialized)
                CoUninitialize();
            return;
        }

        WAVEFORMATEX* mix = nullptr;
        code = client->GetMixFormat(&mix);
        if (SUCCEEDED(code))
        {
            sampleRate = static_cast<int>(mix->nSamplesPerSec);
            channels = mix->nChannels;

            WAVEFORMATEXTENSIBLE format{};
            format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            format.Format.nChannels = mix->nChannels;
            format.Format.nSamplesPerSec = mix->nSamplesPerSec;
            format.Format.wBitsPerSample = 32;
            format.Format.nBlockAlign = static_cast<WORD>(mix->nChannels * 4);
            format.Format.nAvgBytesPerSec = mix->nSamplesPerSec * format.Format.nBlockAlign;
            format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            format.Samples.wValidBitsPerSample = 32;
            format.dwChannelMask = mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE ? reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix)->dwChannelMask
                                 : mix->nChannels == 2                        ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
                                                                              : 0;
            format.SubFormat = SubtypeFloat;
            CoTaskMemFree(mix);

            code = client->Initialize(AUDCLNT_SHAREMODE_SHARED, FlagEventCallback | FlagAutoConvert | FlagSrcQuality, BufferDuration, 0,
                reinterpret_cast<WAVEFORMATEX*>(&format), nullptr);
        }
        if (SUCCEEDED(code))
            code = client->SetEventHandle(renderEvent);
        if (SUCCEEDED(code))
            code = client->GetBufferSize(&bufferFrames);
        if (SUCCEEDED(code))
            code = client->GetService(IidRenderClient, render.Out());
        if (FAILED(code))
        {
            failOpen(HresultText("Starting the audio device", code));
            CloseHandle(renderEvent);
            if (bComInitialized)
                CoUninitialize();
            return;
        }

        ready.release();

        const int frameBytes = GetBytesPerFrame();
        bool bRunning = false;
        bool bLost = false;

        const auto fill = [&]() -> HRESULT {
            UINT32 padding = 0;
            HRESULT result = client->GetCurrentPadding(&padding);
            if (FAILED(result))
                return result;
            queuedFrames = static_cast<int>(padding);

            const UINT32 frames = bufferFrames - padding;
            if (frames == 0)
                return S_OK;

            BYTE* data = nullptr;
            result = render->GetBuffer(frames, &data);
            if (FAILED(result))
                return result;

            const qsizetype wanted = qsizetype(frames) * frameBytes;
            PcmRing* source = ring;
            const qsizetype got = source ? source->Read(reinterpret_cast<char*>(data), wanted) : 0;
            std::memset(data + got, 0, static_cast<size_t>(wanted - got));

            const float gain = volume;
            if (gain != 1.0f)
            {
                auto* samples = reinterpret_cast<float*>(data);
                const qsizetype count = got / static_cast<qsizetype>(sizeof(float));
                for (qsizetype i = 0; i < count; ++i)
                    samples[i] *= gain;
            }

            queuedFrames = static_cast<int>(padding + got / frameBytes);
            return render->ReleaseBuffer(frames, 0);
        };

        const HANDLE handles[] = {stopEvent, commandEvent, renderEvent};
        while (!bLost)
        {
            const DWORD woke = WaitForMultipleObjects(3, handles, FALSE, WaitMs);
            if (woke == WAIT_OBJECT_0)
                break;

            HRESULT result = S_OK;
            const bool bWant = bWantRunning;
            if (bWant != bRunning)
            {
                if (bWant)
                {
                    result = fill();
                    if (SUCCEEDED(result))
                        result = client->Start();
                }
                else
                {
                    result = client->Stop();
                }
                bRunning = bWant;
            }
            else if (bRunning)
            {
                result = fill();
            }

            if (FAILED(result))
            {
                qWarning() << "[Player]" << HresultText("Audio output", result);
                bLost = true;
            }
        }

        if (bRunning && !bLost)
            client->Stop();
        if (bLost)
            emit deviceLost();
    }

    CloseHandle(renderEvent);
    if (bComInitialized)
        CoUninitialize();
}
