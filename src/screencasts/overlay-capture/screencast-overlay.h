/******************************************************************************
    Copyright (C) 2026 by Daniil Nabiulin

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

#ifndef SCREENCAST_OVERLAY_H
#define SCREENCAST_OVERLAY_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <semaphore.h>
#endif

class ScreenCastOverlay : public QThread
{
    Q_OBJECT
public:
    explicit ScreenCastOverlay(QObject *parent = nullptr);
    ~ScreenCastOverlay() override;

    void init();
    void stop();

    void setIsProcessed(bool value) { m_isProcessed = value; }
    void wakeWaitCondition() { QMutexLocker locker(&m_mutex); m_waitCondition.wakeOne(); }

signals:
    void currentFrameBuffer(uint32_t height, uint32_t width, void* data);

public slots:
    void setCurrentFramerate(const QString &framerate) { m_framerate = framerate.toInt(); }

protected:
    void run() override;

private:
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_isProcessed = false;

    int  m_framerate = 15;
    bool m_loop = true;
    bool m_stopped = false;

#ifdef Q_OS_WIN
    HANDLE m_mapping = nullptr;
    HANDLE m_evRequest = nullptr;
    HANDLE m_evReady = nullptr;
#elif defined(Q_OS_LINUX)
    int m_shmFd = -1;
    sem_t *m_semRequest = nullptr;
    sem_t *m_semReady = nullptr;
#endif
    void* m_view = nullptr;

    std::vector<uint8_t> m_frameCopy;

    bool ensureHandles();
    bool ensureMapping();
    bool requestAndReadFrame(uint32_t &outHeight, uint32_t &outWidth, void* &outData);
    void cleanup();
};

#endif // SCREENCAST_OVERLAY_H
