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

#include "screencast-overlay.h"
#include "src/utils/overlay_protocol.h"

#ifdef Q_OS_LINUX
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

ScreenCastOverlay::ScreenCastOverlay(QObject *parent)
    : QThread(parent) {}

ScreenCastOverlay::~ScreenCastOverlay()
{
    cleanup();
}

void ScreenCastOverlay::init()
{
    m_loop = true;
    m_stopped = false;
    m_isProcessed = false;
}

void ScreenCastOverlay::stop()
{
    m_loop = false;
    m_stopped = true;

    m_isProcessed = true;
    m_waitCondition.wakeOne();

    wait();
}

void ScreenCastOverlay::run()
{
    while (m_loop) {
        int frameTime = 1000 / m_framerate;
        msleep(frameTime);

        if (!m_stopped) {
            m_isProcessed = false;
        }

        uint32_t h = 0, w = 0;
        void* data = nullptr;
        const bool got = requestAndReadFrame(h, w, data);

        if (got) {
            emit currentFrameBuffer(h, w, data);

            QMutexLocker locker(&m_mutex);
            while (!m_isProcessed) {
                m_waitCondition.wait(&m_mutex);
            }
        }
    }
}

bool ScreenCastOverlay::ensureHandles()
{
#ifdef Q_OS_WIN
    if (!m_evRequest)
        m_evRequest = CreateEventA(nullptr, FALSE, FALSE, AT_OVERLAY_CAPTURE_REQUEST_EVENT);
    if (!m_evReady)
        m_evReady = CreateEventA(nullptr, FALSE, FALSE, AT_OVERLAY_CAPTURE_READY_EVENT);
    return m_evRequest && m_evReady;
#elif defined(Q_OS_LINUX)
    if (!m_semRequest) {
        sem_t *s = sem_open(AT_OVERLAY_CAPTURE_REQUEST_SEM, O_CREAT, 0600, 0);
        m_semRequest = (s == SEM_FAILED) ? nullptr : s;
    }
    if (!m_semReady) {
        sem_t *s = sem_open(AT_OVERLAY_CAPTURE_READY_SEM, O_CREAT, 0600, 0);
        m_semReady = (s == SEM_FAILED) ? nullptr : s;
        if (m_semReady) {
            while (sem_trywait(m_semReady) == 0) { /* drain */ }
        }
    }
    return m_semRequest && m_semReady;
#else
    return false;
#endif
}

bool ScreenCastOverlay::ensureMapping()
{
    if (m_view) return true;

#ifdef Q_OS_WIN
    if (!m_mapping) {
        m_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, AT_OVERLAY_FRAME_MAPPING);
        if (!m_mapping) return false;
    }
    m_view = MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, 0);
    if (!m_view) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    if (m_shmFd < 0) {
        m_shmFd = shm_open(AT_OVERLAY_FRAME_MAPPING_POSIX, O_RDONLY, 0);
        if (m_shmFd < 0) return false;
    }
    m_view = mmap(nullptr, (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE, PROT_READ, MAP_SHARED, m_shmFd, 0);
    if (m_view == MAP_FAILED) {
        m_view = nullptr;
        close(m_shmFd);
        m_shmFd = -1;
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool ScreenCastOverlay::requestAndReadFrame(uint32_t &outHeight, uint32_t &outWidth, void* &outData)
{
    if (!ensureHandles()) return false;

#ifdef Q_OS_WIN
    SetEvent(m_evRequest);
    if (WaitForSingleObject(m_evReady, 500) != WAIT_OBJECT_0) return false;
#elif defined(Q_OS_LINUX)
    sem_post(m_semRequest);
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += 500 * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec += 1; deadline.tv_nsec -= 1000000000L; }
    if (sem_timedwait(m_semReady, &deadline) != 0) return false;
#else
    return false;
#endif

    if (!ensureMapping()) return false;

    auto* hdr = reinterpret_cast<const at_overlay_frame_header*>(m_view);
    const uint32_t width  = hdr->width;
    const uint32_t height = hdr->height;
    const uint32_t stride = hdr->stride;
    if (width == 0 || height == 0 ||
        width > AT_OVERLAY_FRAME_MAX_WIDTH || height > AT_OVERLAY_FRAME_MAX_HEIGHT || stride < width * 4 ||
        AT_OVERLAY_FRAME_HEADER_SIZE + (size_t)stride * height > (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE) {
        return false;
    }

    const uint32_t dstPitch = width * 4;
    m_frameCopy.resize((size_t)dstPitch * height);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(m_view) + AT_OVERLAY_FRAME_HEADER_SIZE;
    for (uint32_t y = 0; y < height; ++y) {
        memcpy(m_frameCopy.data() + (size_t)y * dstPitch, src + (size_t)y * stride, dstPitch);
    }

    outHeight = height;
    outWidth = width;
    outData = m_frameCopy.data();
    return true;
}

void ScreenCastOverlay::cleanup()
{
#ifdef Q_OS_WIN
    if (m_view) { UnmapViewOfFile(m_view); m_view = nullptr; }
    if (m_mapping) { CloseHandle(m_mapping); m_mapping = nullptr; }
    if (m_evRequest) { CloseHandle(m_evRequest); m_evRequest = nullptr; }
    if (m_evReady) { CloseHandle(m_evReady); m_evReady = nullptr; }
#elif defined(Q_OS_LINUX)
    if (m_view) { munmap(m_view, (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE); m_view = nullptr; }
    if (m_shmFd >= 0) { close(m_shmFd); m_shmFd = -1; }
    if (m_semRequest) { sem_close(m_semRequest); m_semRequest = nullptr; }
    if (m_semReady) { sem_close(m_semReady); m_semReady = nullptr; }
#endif
}
