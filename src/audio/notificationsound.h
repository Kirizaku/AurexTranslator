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

#ifndef NOTIFICATIONSOUND_H
#define NOTIFICATIONSOUND_H

#include <QObject>
#include <QByteArray>

class QAudioSink;
class QBuffer;

class NotificationSound : public QObject
{
    Q_OBJECT

public:
    explicit NotificationSound(QObject *parent = nullptr);
    ~NotificationSound() override;

    void playEnabled();
    void playDisabled();

private:
    struct Tone {
        double frequency;
        int milliseconds;
    };

    void play(const QList<Tone> &tones);
    void stop();

    void appendTone(QByteArray &pcm, double frequency, int milliseconds, int sampleRate);

    static constexpr int kSampleRate = 44100;

    QAudioSink *m_sink = nullptr;
    QBuffer *m_buffer = nullptr;
};

#endif // NOTIFICATIONSOUND_H
