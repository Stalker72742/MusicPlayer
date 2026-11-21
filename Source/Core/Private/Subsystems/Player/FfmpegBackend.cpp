//
// Created by Stalker7274 on 05.11.2026.
//

#include "FfmpegBackend.h"

#include <QDebug>
#include <QMutex>
#include <QScopeGuard>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "PcmRing.h"
#include "Player/AudioDevices.h"
#include "WasapiOutput.h"

namespace
{
    constexpr int RingSeconds = 4;
    constexpr int TickMs = 100;
    constexpr int IoBufferSize = 64 * 1024;
    constexpr int DrainTicks = 3;

    QString ErrorText(int code)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(code, buffer, sizeof(buffer));
        return QString::fromUtf8(buffer);
    }
}

class FfmpegWorker : public QThread
{
    Q_OBJECT

public:
    FfmpegWorker(const QUrl& source, int sampleRate, int channels, PcmRing& ring, qint64 startMs, bool bSourceData, QObject* parent)
        : QThread(parent)
        , location(source.isLocalFile() ? source.toLocalFile().toUtf8() : source.toEncoded())
        , sampleRate(sampleRate)
        , channels(channels)
        , ring(ring)
        , seekMs(startMs > 0 ? startMs : -1)
        , bSourceData(bSourceData)
    {
    }

    void RequestSeek(qint64 positionMs)
    {
        seekMs = positionMs;
        bInterrupted = true;
        ring.Wake();
        WakeEnd();
    }

    void RequestStop()
    {
        bStop = true;
        bInterrupted = true;
        ring.Wake();
        WakeEnd();
    }

signals:
    void opened(qint64 durationMs);
    void failed(const QString& message);
    void decodedToEnd();
    void dataRead(qint64 offset, const QByteArray& bytes);
    void sizeKnown(qint64 size);

protected:
    void run() override
    {
        const QString problem = Decode();
        if (!problem.isEmpty() && !bStop)
            emit failed(problem);
    }

private:
    void WakeEnd()
    {
        QMutexLocker lock(&endMutex);
        endWait.wakeAll();
    }

    static int ReadPacket(void* opaque, uint8_t* buffer, int size)
    {
        auto* self = static_cast<FfmpegWorker*>(opaque);
        const int read = avio_read_partial(self->input, buffer, size);
        if (read > 0)
        {
            if (self->bSourceData)
                emit self->dataRead(self->offset, QByteArray(reinterpret_cast<const char*>(buffer), read));
            self->offset += read;
        }
        return read == 0 ? AVERROR_EOF : read;
    }

    static int64_t SeekPacket(void* opaque, int64_t position, int whence)
    {
        auto* self = static_cast<FfmpegWorker*>(opaque);
        if (whence & AVSEEK_SIZE)
            return avio_size(self->input);

        const int64_t result = avio_seek(self->input, position, whence & ~AVSEEK_FORCE);
        if (result >= 0)
            self->offset = result;
        return result;
    }

    static int ShouldInterrupt(void* opaque)
    {
        return static_cast<FfmpegWorker*>(opaque)->bStop ? 1 : 0;
    }

    QString Decode()
    {
        const AVIOInterruptCB interrupt{&FfmpegWorker::ShouldInterrupt, this};

        AVDictionary* options = nullptr;
        if (location.contains("://"))
        {
            av_dict_set(&options, "reconnect", "1", 0);
            av_dict_set(&options, "reconnect_on_network_error", "1", 0);
            av_dict_set(&options, "reconnect_delay_max", "5", 0);
        }

        int code = avio_open2(&input, location.constData(), AVIO_FLAG_READ, &interrupt, &options);
        av_dict_free(&options);
        if (code < 0)
            return QStringLiteral("Cannot open the source: ") + ErrorText(code);
        const auto closeInput = qScopeGuard([this] { avio_closep(&input); });

        const int64_t size = avio_size(input);
        if (size > 0)
            emit sizeKnown(size);

        auto* ioBuffer = static_cast<unsigned char*>(av_malloc(IoBufferSize));
        AVIOContext* io = avio_alloc_context(ioBuffer, IoBufferSize, 0, this, &FfmpegWorker::ReadPacket, nullptr, &FfmpegWorker::SeekPacket);
        const auto closeIo = qScopeGuard([&io] {
            if (io)
                av_freep(&io->buffer);
            avio_context_free(&io);
        });

        AVFormatContext* container = avformat_alloc_context();
        container->pb = io;
        container->flags |= AVFMT_FLAG_CUSTOM_IO;
        container->interrupt_callback = interrupt;
        code = avformat_open_input(&container, "", nullptr, nullptr);
        if (code < 0)
            return QStringLiteral("Cannot read the source: ") + ErrorText(code);
        const auto closeContainer = qScopeGuard([&container] { avformat_close_input(&container); });

        code = avformat_find_stream_info(container, nullptr);
        if (code < 0)
            return QStringLiteral("Cannot read the source: ") + ErrorText(code);

        const AVCodec* decoder = nullptr;
        const int streamIndex = av_find_best_stream(container, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
        if (streamIndex < 0 || !decoder)
            return QStringLiteral("No audio this player can decode in the source");
        const AVStream* stream = container->streams[streamIndex];

        AVCodecContext* codec = avcodec_alloc_context3(decoder);
        const auto closeCodec = qScopeGuard([&codec] { avcodec_free_context(&codec); });
        avcodec_parameters_to_context(codec, stream->codecpar);
        code = avcodec_open2(codec, decoder, nullptr);
        if (code < 0)
            return QStringLiteral("Cannot decode the source: ") + ErrorText(code);

        AVChannelLayout outLayout{};
        av_channel_layout_default(&outLayout, channels);
        SwrContext* resampler = nullptr;
        code = swr_alloc_set_opts2(&resampler, &outLayout, AV_SAMPLE_FMT_FLT, sampleRate,
            &codec->ch_layout, codec->sample_fmt, codec->sample_rate, 0, nullptr);
        if (code < 0 || (code = swr_init(resampler)) < 0)
        {
            swr_free(&resampler);
            return QStringLiteral("Cannot convert the audio: ") + ErrorText(code);
        }
        const auto closeResampler = qScopeGuard([&resampler] { swr_free(&resampler); });

        qint64 durationMs = 0;
        if (container->duration > 0)
            durationMs = container->duration / (AV_TIME_BASE / 1000);
        emit opened(durationMs);

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        const auto freeFrames = qScopeGuard([&packet, &frame] {
            av_packet_free(&packet);
            av_frame_free(&frame);
        });
        QByteArray converted;
        const int frameBytes = channels * static_cast<int>(sizeof(float));

        qint64 dropBeforeMs = -1;
        bool bEnded = false;
        bool bEndReported = false;

        while (!bStop)
        {
            const qint64 target = seekMs.exchange(-1);
            bInterrupted = false;
            if (target >= 0)
            {
                const int64_t timestamp = av_rescale_q(target, AVRational{1, 1000}, stream->time_base);
                code = av_seek_frame(container, streamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
                if (code < 0)
                    qWarning() << "[Player] Seek failed:" << ErrorText(code);
                avcodec_flush_buffers(codec);
                swr_init(resampler);
                dropBeforeMs = target;
                bEnded = false;
                bEndReported = false;
            }

            if (bEnded)
            {
                if (!bEndReported)
                {
                    bEndReported = true;
                    emit decodedToEnd();
                }
                QMutexLocker lock(&endMutex);
                if (!bStop && seekMs < 0)
                    endWait.wait(&endMutex, 100);
                continue;
            }

            code = av_read_frame(container, packet);
            if (code == AVERROR_EOF)
            {
                bEnded = true;
                avcodec_send_packet(codec, nullptr);
            }
            else if (code < 0)
            {
                if (bStop)
                    break;
                return QStringLiteral("Cannot read the source: ") + ErrorText(code);
            }
            else if (packet->stream_index != streamIndex)
            {
                av_packet_unref(packet);
                continue;
            }
            else
            {
                code = avcodec_send_packet(codec, packet);
                av_packet_unref(packet);
                if (code < 0 && code != AVERROR(EAGAIN))
                    continue;
            }

            while (!bStop && seekMs < 0 && avcodec_receive_frame(codec, frame) == 0)
            {
                if (dropBeforeMs >= 0 && frame->pts != AV_NOPTS_VALUE)
                {
                    const qint64 frameMs = av_rescale_q(frame->pts, stream->time_base, AVRational{1, 1000});
                    const qint64 frameLengthMs = frame->nb_samples * 1000 / std::max(1, frame->sample_rate);
                    if (frameMs + frameLengthMs < dropBeforeMs)
                    {
                        av_frame_unref(frame);
                        continue;
                    }
                    dropBeforeMs = -1;
                }

                const int capacity = swr_get_out_samples(resampler, frame->nb_samples);
                converted.resize(capacity * frameBytes);
                auto* out = reinterpret_cast<uint8_t*>(converted.data());
                const int samples = swr_convert(resampler, &out, capacity, frame->extended_data, frame->nb_samples);
                av_frame_unref(frame);
                if (samples > 0)
                    ring.Write(converted.constData(), samples * frameBytes, bInterrupted);
            }
        }
        return {};
    }

    const QByteArray location;
    const int sampleRate;
    const int channels;
    PcmRing& ring;
    std::atomic<qint64> seekMs;
    std::atomic<bool> bInterrupted{false};
    std::atomic<bool> bStop{false};
    const bool bSourceData;

    AVIOContext* input = nullptr;
    qint64 offset = 0;
    QMutex endMutex;
    QWaitCondition endWait;
};

FfmpegBackend::FfmpegBackend(QObject* parent)
    : AudioBackend(parent)
{
    av_log_set_level(AV_LOG_QUIET);

    tick.setInterval(TickMs);
    connect(&tick, &QTimer::timeout, this, &FfmpegBackend::OnTick);
}

FfmpegBackend::~FfmpegBackend()
{
    StopWorker();
}

void FfmpegBackend::StopWorker()
{
    ++generation;
    if (worker)
    {
        worker->RequestStop();
        worker->wait();
        delete worker;
        worker = nullptr;
    }
    output.reset();
    ring.reset();
}

bool FfmpegBackend::StartWorker(qint64 startMs)
{
    StopWorker();

    const quint64 current = generation;

    output = std::make_unique<WasapiOutput>();
    const QString problem = output->Open(outputDevice);
    if (!problem.isEmpty())
    {
        output.reset();
        emit errorOccurred(problem);
        return false;
    }

    ring = std::make_unique<PcmRing>(qsizetype(output->GetSampleRate()) * output->GetBytesPerFrame() * RingSeconds);
    output->SetRing(ring.get());
    output->SetVolume(volume);
    connect(output.get(), &WasapiOutput::deviceLost, this, [this, current] {
        if (current == generation)
            Reopen();
    }, Qt::QueuedConnection);

    baseMs = startMs;
    bDecoderFinished = false;
    drainTicks = 0;

    worker = new FfmpegWorker(source, output->GetSampleRate(), output->GetChannels(), *ring, startMs, bSourceData, this);

    connect(worker, &FfmpegWorker::opened, this, [this, current](qint64 durationMs) {
        if (current != generation)
            return;
        duration = durationMs;
        emit durationChanged(duration);
        emit loaded();
    });
    connect(worker, &FfmpegWorker::failed, this, [this, current](const QString& message) {
        if (current == generation)
            emit errorOccurred(message);
    });
    connect(worker, &FfmpegWorker::decodedToEnd, this, [this, current] {
        if (current == generation)
            bDecoderFinished = true;
    });
    connect(worker, &FfmpegWorker::dataRead, this, [this, current](qint64 offset, const QByteArray& bytes) {
        if (current == generation)
            emit sourceDataReceived(offset, bytes);
    });
    connect(worker, &FfmpegWorker::sizeKnown, this, [this, current](qint64 size) {
        if (current == generation)
            emit sourceSizeKnown(size);
    });

    worker->start();
    return true;
}

void FfmpegBackend::Reopen()
{
    if (source.isEmpty())
        return;

    const qint64 position = GetPosition();
    const State previous = state;
    if (!StartWorker(position))
    {
        tick.stop();
        SetState(State::Stopped);
        return;
    }
    if (previous == State::Playing)
        output->Start();
}

void FfmpegBackend::SetSource(const QUrl& newSource)
{
    StopWorker();
    tick.stop();

    source = newSource;
    duration = 0;
    baseMs = 0;
    lastPosition = -1;
    SetState(State::Stopped);
    emit durationChanged(0);

    if (!source.isEmpty())
        StartWorker(0);
}

void FfmpegBackend::Play()
{
    if (source.isEmpty() || (!output && !StartWorker(0)))
        return;

    output->Start();
    tick.start();
    SetState(State::Playing);
}

void FfmpegBackend::Pause()
{
    if (state != State::Playing)
        return;

    if (output)
        output->Pause();
    tick.stop();
    SetState(State::Paused);
}

void FfmpegBackend::Stop()
{
    if (output)
        output->Pause();
    tick.stop();
    SetState(State::Stopped);
    Seek(0);
}

void FfmpegBackend::Seek(qint64 positionMs)
{
    if (!worker || !ring)
        return;

    const qint64 target = std::max<qint64>(0, positionMs);
    baseMs = target;
    bDecoderFinished = false;
    drainTicks = 0;
    worker->RequestSeek(target);
    ring->Clear();
    emit positionChanged(target);
}

qint64 FfmpegBackend::GetPosition() const
{
    if (!ring || !output || output->GetBytesPerFrame() == 0)
        return baseMs;

    const qint64 frames = ring->TotalRead() / output->GetBytesPerFrame() - output->GetQueuedFrames();
    return baseMs + std::max<qint64>(0, frames) * 1000 / output->GetSampleRate();
}

void FfmpegBackend::SetVolume(float linear)
{
    volume = linear;
    if (output)
        output->SetVolume(volume);
}

void FfmpegBackend::SetOutputDevice(const QByteArray& deviceId)
{
    outputDevice = deviceId;
    if (output && output->GetEndpoint() != AudioDevices::Resolve(deviceId))
        Reopen();
}

void FfmpegBackend::SetState(State value)
{
    if (state == value)
        return;
    state = value;
    emit stateChanged();
}

void FfmpegBackend::OnTick()
{
    const qint64 position = GetPosition();
    if (position != lastPosition)
    {
        lastPosition = position;
        emit positionChanged(position);
    }

    if (bDecoderFinished && ring && ring->IsEmpty() && ++drainTicks >= DrainTicks)
    {
        bDecoderFinished = false;
        drainTicks = 0;
        emit finished();
    }
}

#include "FfmpegBackend.moc"
