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

#ifndef CAPTURECONTROLLER_H
#define CAPTURECONTROLLER_H

#include <QObject>
#include <cstdint>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_LINUX
class Pipewire;
class ScreenCastPortal;
#endif
class ScreenCast;
class ScreenCastWindow;

class CaptureController : public QObject
{
    Q_OBJECT

public:
    explicit CaptureController(QObject *parent = nullptr);
    ~CaptureController();

    // - Linux: Pipewire + XDG ScreenCast Portal, with fallback to X11 ScreenCast
    //   if Portal fails (e.g. running under bare X11 session)
    // - Windows: ScreenCast (DXGI/GDI)
    void initialize(const QString &restoreToken);

    void setCaptureDesktop(bool isDesktop);
    void setDisplayIndex(int index);
#ifdef Q_OS_WIN
    void setCurrentWindow(HWND hwnd);
#else
    void setCurrentWindow(unsigned long windowId);
#endif
    void setFramerate(const QString &framerate);

    void start();
    void stop();

    // Frame consumer (OpenCV) is done processing the last frame; allow the
    // producer thread to deliver the next one. Bridges backend's wait condition
    void notifyFrameProcessed();

    // User clicked "Select source". On Portal: trigger a new session selector dialog
    void openSourceSelector();

    ScreenCastWindow* screenCastWindow() const { return m_screenCastWindow; }

signals:
    // Raw frame buffer from any active backend
    void frameBufferReady(uint32_t height, uint32_t width, void* data);

    // Portal: a new restore token was issued
    void restoreTokenChanged(const QString &token);

    void captureFinished();
    void aboutToReconfigure();
    void captureRestarted();

private slots:
#ifdef Q_OS_LINUX
    void onPortalNodeId(const uint &nodeId);
    void onPortalRestoreToken(const QString &token);
    void onPortalFailed();
#endif

private:
#ifdef Q_OS_LINUX
    Pipewire *m_pipewire = nullptr;
    ScreenCastPortal *m_portalScreencast = nullptr;
#endif
    ScreenCast *m_screenCapture = nullptr;
    ScreenCastWindow *m_screenCastWindow = nullptr;

    bool m_isCaptureDesktop = true;
    int m_currentDisplay = 0;
#ifdef Q_OS_WIN
    HWND m_currentWindow = nullptr;
#else
    unsigned long m_currentWindow = 0;
#endif
    QString m_framerate;
    QString m_restoreToken;
    bool m_initialized = false;

    void initFallbackBackend();
};

#endif // CAPTURECONTROLLER_H
