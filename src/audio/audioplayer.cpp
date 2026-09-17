/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "audioplayer.h"
#include "src/utils/logger.h"

#ifdef Q_OS_LINUX
#include "src/audio/pipewiredefaultsinkwatcher.h"
#endif

#include <QAudioDecoder>
#include <QAudioSink>
#include <QBuffer>
#include <QMediaDevices>
#include <QtEndian>

namespace {

quint32 readU32(const QByteArray &data, int offset)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData()) + offset);
}

quint16 readU16(const QByteArray &data, int offset)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData()) + offset);
}

QByteArray duplicateChannel(const QByteArray &mono, int bytesPerSample)
{
    if (bytesPerSample <= 0)
        return mono;

    const int frames = mono.size() / bytesPerSample;

    QByteArray stereo;
    stereo.resize(frames * bytesPerSample * 2);

    const char *src = mono.constData();
    char *dst = stereo.data();

    for (int i = 0; i < frames; ++i) {
        std::memcpy(dst, src, bytesPerSample);
        std::memcpy(dst + bytesPerSample, src, bytesPerSample);
        src += bytesPerSample;
        dst += 2 * bytesPerSample;
    }

    return stereo;
}

QByteArray resampleInt16(const QByteArray &samples, int channelCount, int fromRate, int toRate)
{
    if (channelCount <= 0 || fromRate <= 0 || toRate <= 0)
        return samples;

    const int frameBytes = int(sizeof(qint16)) * channelCount;
    const int srcFrames = samples.size() / frameBytes;
    if (srcFrames <= 0)
        return samples;

    const auto *src = reinterpret_cast<const qint16 *>(samples.constData());
    const int dstFrames = int(qint64(srcFrames) * toRate / fromRate);

    QByteArray resampled;
    resampled.resize(dstFrames * frameBytes);
    auto *dst = reinterpret_cast<qint16 *>(resampled.data());

    for (int i = 0; i < dstFrames; ++i) {
        const double srcPos = double(i) * fromRate / toRate;
        const int frame0 = int(srcPos);
        const int frame1 = qMin(frame0 + 1, srcFrames - 1);
        const double t = srcPos - frame0;

        for (int ch = 0; ch < channelCount; ++ch) {
            const qint16 s0 = src[frame0 * channelCount + ch];
            const qint16 s1 = src[frame1 * channelCount + ch];
            dst[i * channelCount + ch] = static_cast<qint16>(s0 + (s1 - s0) * t);
        }
    }

    return resampled;
}

QByteArray floatToInt16(const QByteArray &samples)
{
    const int count = samples.size() / int(sizeof(float));
    const float *src = reinterpret_cast<const float *>(samples.constData());

    QByteArray converted;
    converted.resize(count * int(sizeof(qint16)));
    qint16 *dst = reinterpret_cast<qint16 *>(converted.data());

    for (int i = 0; i < count; ++i)
        dst[i] = static_cast<qint16>(qBound(-1.0f, src[i], 1.0f) * 32767.0f);

    return converted;
}

} // namespace

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_LINUX
    m_defaultSinkWatcher = new PipeWireDefaultSinkWatcher(this);
    connect(m_defaultSinkWatcher, &PipeWireDefaultSinkWatcher::defaultSinkChanged, this, [this](const QString &description) {
        if (!m_playing || !m_deviceName.isEmpty())
            return;

        for (const QAudioDevice &candidate : QMediaDevices::audioOutputs()) {
            if (candidate.description() == description) {
                switchToDevice(candidate);
                break;
            }
        }
    });
#endif
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

void AudioPlayer::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);

    if (m_sink)
        m_sink->setVolume(QAudio::convertVolume(m_volume / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));
}

void AudioPlayer::setDeviceName(const QString &name)
{
    if (m_deviceName == name)
        return;
    m_deviceName = name;

    switchToDevice(resolveDevice());
}

void AudioPlayer::play(const QByteArray &audio)
{
    stop();

    if (audio.isEmpty()) {
        Log(Logger::Level::Warning, QStringLiteral("[speech] play: empty audio buffer"));
        emit errorOccurred(tr("Unsupported audio data."));
        return;
    }

    if (audio.startsWith("RIFF")) {
        QAudioFormat format;
        QByteArray pcm;

        if (!parseWav(audio, format, pcm)) {
            Log(Logger::Level::Warning, QStringLiteral("[speech] play: RIFF header not understood (%1 bytes)").arg(audio.size()));
            emit errorOccurred(tr("Unsupported audio data."));
            return;
        }

        playPcm(format, pcm);
        return;
    }

    decode(audio);
}

void AudioPlayer::stop()
{
    disposeDecoder();

    if (m_sink) {
        QAudioSink *sink = m_sink;
        m_sink = nullptr;
        sink->disconnect(this);
        sink->stop();
        sink->deleteLater();
    }

    if (m_buffer) {
        m_buffer->close();
        m_buffer->deleteLater();
        m_buffer = nullptr;
    }

    setPlaying(false);
}

bool AudioPlayer::parseWav(const QByteArray &wav, QAudioFormat &format, QByteArray &pcm)
{
    if (wav.size() < 44 || !wav.startsWith("RIFF") || wav.mid(8, 4) != "WAVE")
        return false;

    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;
    bool haveFormat = false;

    int offset = 12;
    while (offset + 8 <= wav.size()) {
        const QByteArray id = wav.mid(offset, 4);
        const quint32 size = readU32(wav, offset + 4);
        const int body = offset + 8;

        if (id == "fmt " && size >= 16 && body + 16 <= wav.size()) {
            channels = readU16(wav, body + 2);
            sampleRate = static_cast<int>(readU32(wav, body + 4));
            bitsPerSample = readU16(wav, body + 14);
            haveFormat = true;
        } else if (id == "data") {
            const int available = qMin<qint64>(size, wav.size() - body);
            pcm = wav.mid(body, available);
            break;
        }

        offset = body + static_cast<int>(size) + (size % 2);
    }

    if (!haveFormat || pcm.isEmpty() || sampleRate <= 0 || channels <= 0)
        return false;

    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);

    switch (bitsPerSample) {
    case 8:
        format.setSampleFormat(QAudioFormat::UInt8);
        break;
    case 16:
        format.setSampleFormat(QAudioFormat::Int16);
        break;
    case 32:
        format.setSampleFormat(QAudioFormat::Int32);
        break;
    default:
        return false;
    }

    return true;
}

void AudioPlayer::decode(const QByteArray &audio)
{
    m_source = new QBuffer(this);
    m_source->setData(audio);
    m_source->open(QIODevice::ReadOnly);

    m_decoded.clear();
    m_decodedFormat = QAudioFormat();

    m_decoder = new QAudioDecoder(this);
    m_decoder->setSourceDevice(m_source);

    connect(m_decoder, &QAudioDecoder::bufferReady, this, [this] {
        const QAudioBuffer buffer = m_decoder->read();
        if (!buffer.isValid())
            return;

        m_decodedFormat = buffer.format();
        m_decoded.append(buffer.constData<char>(), buffer.byteCount());
    });

    connect(m_decoder, &QAudioDecoder::finished, this, [this] {
        const QAudioFormat format = m_decodedFormat;
        const QByteArray pcm = m_decoded;

        disposeDecoder();

        if (!format.isValid() || pcm.isEmpty()) {
            Log(Logger::Level::Warning,
                QStringLiteral("[speech] decoder finished with nothing usable (format valid: %1, %2 bytes)")
                    .arg(format.isValid() ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(pcm.size()));
            setPlaying(false);
            emit errorOccurred(tr("The audio could not be decoded."));
            return;
        }

        playPcm(format, pcm);
    });

    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) {
                const QString message = m_decoder->errorString();

                Log(Logger::Level::Warning, QStringLiteral("[speech] decoder: ") + message);

                disposeDecoder();
                setPlaying(false);
                emit errorOccurred(message);
            });

    m_decoder->start();
    setPlaying(true);
}

void AudioPlayer::disposeDecoder()
{
    if (m_decoder) {
        QAudioDecoder *decoder = m_decoder;
        m_decoder = nullptr;

        decoder->disconnect(this);
        decoder->stop();
        decoder->deleteLater();
    }

    if (m_source) {
        m_source->close();
        m_source->deleteLater();
        m_source = nullptr;
    }

    m_decoded.clear();
    m_decodedFormat = QAudioFormat();
}

void AudioPlayer::playPcm(QAudioFormat format, QByteArray pcm)
{
    const QAudioDevice device = resolveDevice();
    if (device.isNull()) {
        Log(Logger::Level::Warning, QStringLiteral("[speech] playPcm: no audio output device"));
        setPlaying(false);
        emit errorOccurred(tr("No audio output device."));
        return;
    }

    if (format.sampleFormat() == QAudioFormat::Float) {
        pcm = floatToInt16(pcm);
        format.setSampleFormat(QAudioFormat::Int16);
    }

    const QAudioFormat preferred = device.preferredFormat();
    const int targetRate = preferred.sampleRate() > 0 ? preferred.sampleRate() : format.sampleRate();

    if (format.sampleFormat() == QAudioFormat::Int16 && format.sampleRate() != targetRate) {
        pcm = resampleInt16(pcm, format.channelCount(), format.sampleRate(), targetRate);
        format.setSampleRate(targetRate);
    }

    if (format.channelCount() == 1) {
        pcm = duplicateChannel(pcm, format.bytesPerSample());
        format.setChannelCount(2);
    }

    m_buffer = new QBuffer(this);
    m_buffer->setData(pcm);
    m_buffer->open(QIODevice::ReadOnly);

    createSink(device, format);
    m_sink->setVolume(QAudio::convertVolume(m_volume / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));

    setPlaying(true);
    m_sink->start(m_buffer);
}

void AudioPlayer::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    emit playingChanged(playing);
}

QAudioDevice AudioPlayer::resolveDevice() const
{
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    const auto byName = [&outputs](const QString &name) {
        if (!name.isEmpty()) {
            for (const QAudioDevice &device : outputs) {
                if (device.description() == name)
                    return device;
            }
        }
        return QAudioDevice();
    };

    if (!m_deviceName.isEmpty()) {
        const QAudioDevice chosen = byName(m_deviceName);
        if (!chosen.isNull())
            return chosen;

        Log(Logger::Level::Warning, QStringLiteral("[speech] audio device \"%1\" is not connected, using the system default").arg(m_deviceName));
    }

#ifdef Q_OS_LINUX
    // Qt's default output doesn't follow changes made while the app runs (and Qt 6.2's
    // GStreamer backend just reports the first device it found), so ask PipeWire directly.
    // Confirmed broken on Qt 6.2.4 and 6.4.2; a newer Qt on Arch didn't show the bug, but
    // I still ship builds against the older Qt versions, so keep this regardless
    const QAudioDevice systemDefault = byName(m_defaultSinkWatcher->currentDescription());
    if (!systemDefault.isNull())
        return systemDefault;
#endif

    return QMediaDevices::defaultAudioOutput();
}

void AudioPlayer::createSink(const QAudioDevice &device, const QAudioFormat &format)
{
    m_activeDeviceDescription = device.description();
    m_sink = new QAudioSink(device, format, this);

    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && m_sink && m_sink->error() != QAudio::NoError) {
            const int errorCode = static_cast<int>(m_sink->error());
            Log(Logger::Level::Warning, QStringLiteral("[speech] playPcm: sink error %1").arg(errorCode));
            emit errorOccurred(tr("Playback failed (error %1).").arg(errorCode));
        }

        if (state == QAudio::IdleState || state == QAudio::StoppedState)
            stop();
    });
}

void AudioPlayer::switchToDevice(const QAudioDevice &device)
{
    if (!m_playing || !m_sink || !m_buffer || device.isNull())
        return;

    if (device.description() == m_activeDeviceDescription)
        return;

    const QAudioFormat format = m_sink->format();
    const qreal volume = m_sink->volume();

    QAudioSink *oldSink = m_sink;
    m_sink = nullptr;
    oldSink->disconnect(this);
    oldSink->stop();
    oldSink->deleteLater();

    createSink(device, format);
    m_sink->setVolume(volume);
    m_sink->start(m_buffer);
}
