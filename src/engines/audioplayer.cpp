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
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        Log(Logger::Level::Warning, QStringLiteral("[speech] playPcm: no default audio output device"));
        setPlaying(false);
        emit errorOccurred(tr("No audio output device."));
        return;
    }

    if (format.sampleFormat() == QAudioFormat::Float && !device.isFormatSupported(format)) {
        QAudioFormat integer = format;
        integer.setSampleFormat(QAudioFormat::Int16);

        if (device.isFormatSupported(integer)) {
            pcm = floatToInt16(pcm);
            format = integer;
        }
    }

    if (format.channelCount() == 1) {
        QAudioFormat stereo = format;
        stereo.setChannelCount(2);

        if (device.isFormatSupported(stereo)) {
            pcm = duplicateChannel(pcm, format.bytesPerSample());
            format = stereo;
        }
    }

    if (!device.isFormatSupported(format)) {
        Log(Logger::Level::Warning,
            QStringLiteral("[speech] playPcm: %1 rejects %2 Hz, %3 ch, sample format %4")
                .arg(device.description())
                .arg(format.sampleRate())
                .arg(format.channelCount())
                .arg(static_cast<int>(format.sampleFormat())));
        setPlaying(false);
        emit errorOccurred(tr("The device does not support %1 Hz, %2 channels.")
                               .arg(format.sampleRate())
                               .arg(format.channelCount()));
        return;
    }

    m_buffer = new QBuffer(this);
    m_buffer->setData(pcm);
    m_buffer->open(QIODevice::ReadOnly);

    m_sink = new QAudioSink(device, format, this);
    m_sink->setVolume(QAudio::convertVolume(m_volume / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));

    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState || state == QAudio::StoppedState)
            stop();
    });

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
