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

#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>
#include <QAudioDevice>
#include <QAudioFormat>

class QAudioDecoder;
class QAudioSink;
class QBuffer;

#ifdef Q_OS_LINUX
class PipeWireDefaultSinkWatcher;
#endif

class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    bool isPlaying() const { return m_playing; }

    int volume() const { return m_volume; }
    void setVolume(int percent);
    void setDeviceName(const QString &name);

    void play(const QByteArray &audio);
    void stop();

signals:
    void playingChanged(bool playing);
    void errorOccurred(const QString &error);

private:
    static bool parseWav(const QByteArray &wav, QAudioFormat &format, QByteArray &pcm);

    void decode(const QByteArray &audio);
    void disposeDecoder();
    void playPcm(QAudioFormat format, QByteArray pcm);
    void setPlaying(bool playing);
    QAudioDevice resolveDevice() const;
    void createSink(const QAudioDevice &device, const QAudioFormat &format);
    void switchToDevice(const QAudioDevice &device);

    QString m_deviceName;
    QString m_activeDeviceDescription;
    QAudioSink *m_sink = nullptr;
    QBuffer *m_buffer = nullptr;

#ifdef Q_OS_LINUX
    PipeWireDefaultSinkWatcher *m_defaultSinkWatcher = nullptr;
#endif

    QAudioDecoder *m_decoder = nullptr;
    QBuffer *m_source = nullptr;
    QByteArray m_decoded;
    QAudioFormat m_decodedFormat;

    int m_volume = 100;
    bool m_playing = false;
};

#endif // AUDIOPLAYER_H
