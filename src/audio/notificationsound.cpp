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

#include "notificationsound.h"

#include <QAudioSink>
#include <QBuffer>
#include <QMediaDevices>

NotificationSound::NotificationSound(QObject *parent)
    : QObject(parent)
{}

NotificationSound::~NotificationSound()
{
    stop();
}

void NotificationSound::playEnabled()
{
    play({{660.0, 80}, {880.0, 110}});
}

void NotificationSound::playDisabled()
{
    play({{440.0, 160}});
}

void NotificationSound::play(const QList<Tone> &tones)
{
    stop();

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull())
        return;

    QByteArray mono;
    for (const Tone &tone : tones)
        appendTone(mono, tone.frequency, tone.milliseconds, kSampleRate);

    if (mono.isEmpty())
        return;

    mono.append(kSampleRate * 60 / 1000 * int(sizeof(qint16)), '\0');

    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioFormat stereo = format;
    stereo.setChannelCount(2);

    QAudioFormat monoFormat = format;
    monoFormat.setChannelCount(1);

    QByteArray pcm;
    if (device.isFormatSupported(stereo)) {
        format = stereo;

        const int frames = mono.size() / int(sizeof(qint16));
        pcm.resize(frames * 2 * int(sizeof(qint16)));
        const auto *src = reinterpret_cast<const qint16 *>(mono.constData());
        auto *dst = reinterpret_cast<qint16 *>(pcm.data());
        for (int i = 0; i < frames; ++i) {
            dst[2 * i] = src[i];
            dst[2 * i + 1] = src[i];
        }
    } else if (device.isFormatSupported(monoFormat)) {
        format = monoFormat;
        pcm = mono;
    } else {
        return;
    }

    m_buffer = new QBuffer(this);
    m_buffer->setData(pcm);
    m_buffer->open(QIODevice::ReadOnly);

    m_sink = new QAudioSink(device, format, this);

    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState || state == QAudio::StoppedState)
            stop();
    });

    m_sink->start(m_buffer);
}

void NotificationSound::stop()
{
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
}

void NotificationSound::appendTone(QByteArray &pcm, double frequency, int milliseconds, int sampleRate)
{
    const int frames = sampleRate * milliseconds / 1000;
    const int fade = std::min(frames / 2, sampleRate / 200);

    const int base = pcm.size();
    pcm.resize(base + frames * int(sizeof(qint16)));
    auto *out = reinterpret_cast<qint16 *>(pcm.data() + base);

    for (int i = 0; i < frames; ++i) {
        double gain = 0.35;
        if (i < fade)
            gain *= double(i) / fade;
        else if (i >= frames - fade)
            gain *= double(frames - i) / fade;

        const double sample = std::sin(2.0 * M_PI * frequency * i / sampleRate);
        out[i] = static_cast<qint16>(gain * sample * 32767.0);
    }
}
