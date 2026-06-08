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

#include "capturecontroller.h"

#include "src/utils/logger.h"
#include "src/UI/screencastwindow.h"

#ifdef Q_OS_LINUX
#include "src/screencasts/linux-capture-portal/screencast-pipewire.h"
#include "src/screencasts/linux-capture-portal/screencast-portal.h"
#include "src/screencasts/linux-capture-x11/screencast-x11.h"
#else
#include "src/screencasts/win-capture/screencast-win.h"
#endif

CaptureController::CaptureController(QObject *parent)
    : QObject(parent) {}

CaptureController::~CaptureController()
{
#ifdef Q_OS_LINUX
    if (m_pipewire) {
        m_pipewire->stop();
        delete m_pipewire;
        m_pipewire = nullptr;
    }
    if (m_portalScreencast) {
        delete m_portalScreencast;
        m_portalScreencast = nullptr;
    }
#endif
    if (m_screenCapture) {
        m_screenCapture->stop();
        delete m_screenCapture;
        m_screenCapture = nullptr;
    }
    if (m_screenCastWindow) {
        delete m_screenCastWindow;
        m_screenCastWindow = nullptr;
    }
}

void CaptureController::initialize(const QString &restoreToken)
{
    if (m_initialized) return;
    m_initialized = true;

    m_restoreToken = restoreToken;

#ifdef Q_OS_LINUX
    // Start with Pipewire + Portal. If Portal init fails, fall back to X11.
    m_pipewire = new Pipewire();
    if (!m_framerate.isEmpty()) m_pipewire->setCurrentFramerate(m_framerate);
    connect(m_pipewire, &Pipewire::currentFrameBuffer,
            this, &CaptureController::frameBufferReady);

    m_portalScreencast = new ScreenCastPortal();
    connect(m_portalScreencast, &ScreenCastPortal::currentRestoreToken,
            this, &CaptureController::onPortalRestoreToken);
    connect(m_portalScreencast, &ScreenCastPortal::currentNodeId,
            this, &CaptureController::onPortalNodeId);
    connect(m_portalScreencast, &ScreenCastPortal::failedPortal,
            this, &CaptureController::onPortalFailed);
#else
    initFallbackBackend();
#endif
}

void CaptureController::setCaptureDesktop(bool isDesktop)
{
    m_isCaptureDesktop = isDesktop;
    if (m_screenCapture) m_screenCapture->setIsCaptureDesktop(isDesktop);
}

void CaptureController::setDisplayIndex(int index)
{
    m_currentDisplay = index;
    if (m_screenCapture && m_isCaptureDesktop) {
        m_screenCapture->setCurrentDisplayIndex(index);
    }
}

#ifdef Q_OS_WIN
void CaptureController::setCurrentWindow(HWND hwnd)
{
    m_currentWindow = hwnd;
    if (m_screenCapture && !m_isCaptureDesktop) {
        m_screenCapture->setCurrentWindow(hwnd);
    }
}
#else
void CaptureController::setCurrentWindow(unsigned long windowId)
{
    m_currentWindow = windowId;
    if (m_screenCapture && !m_isCaptureDesktop) {
        m_screenCapture->setCurrentWindow(windowId);
    }
}
#endif

void CaptureController::setFramerate(const QString &framerate)
{
    m_framerate = framerate;
#ifdef Q_OS_LINUX
    if (m_pipewire) m_pipewire->setCurrentFramerate(framerate);
#endif
    if (m_screenCapture) m_screenCapture->setCurrentFramerate(framerate);
}

void CaptureController::start()
{
#ifdef Q_OS_LINUX
    if (m_portalScreencast) {
        m_portalScreencast->init(m_restoreToken);
        return;
    }
#endif
    if (m_screenCapture) {
        m_screenCapture->init();
        m_screenCapture->start();
    }
}

void CaptureController::stop()
{
#ifdef Q_OS_LINUX
    if (m_pipewire) m_pipewire->stop();
    if (m_portalScreencast) m_portalScreencast->stop();
#endif
    if (m_screenCapture) m_screenCapture->stop();
    if (m_screenCastWindow) m_screenCastWindow->hide();
}

void CaptureController::notifyFrameProcessed()
{
#ifdef Q_OS_LINUX
    if (m_pipewire) {
        m_pipewire->setIsProcessed(true);
        m_pipewire->wakeWaitCondition();
    }
#endif
    if (m_screenCapture) {
        m_screenCapture->setIsProcessed(true);
        m_screenCapture->wakeWaitCondition();
    }
}

void CaptureController::openSourceSelector()
{
#ifdef Q_OS_LINUX
    if (m_portalScreencast) {
        emit aboutToReconfigure();
        stop();
        m_portalScreencast->reload();
        return;
    }
#endif
    if (m_screenCastWindow) {
        m_screenCastWindow->show();
    } else {
        Log(Logger::Level::Warning, "[capture] openSourceSelector called but no backend has a source picker yet");
    }
}

#ifdef Q_OS_LINUX
void CaptureController::onPortalNodeId(const uint &nodeId)
{
    Log(Logger::Level::Info, "[pipewire] Source selected");
    if (!m_pipewire) return;
    m_pipewire->init(nodeId);
    m_pipewire->start();
    m_pipewire->setIsStopped(false);

    emit captureRestarted();
}

void CaptureController::onPortalRestoreToken(const QString &token)
{
    m_restoreToken = token;
    emit restoreTokenChanged(token);
}

void CaptureController::onPortalFailed()
{
    Log(Logger::Level::Warning, "[capture] Portal failed, falling back to X11 ScreenCast");

    emit aboutToReconfigure();

    if (m_portalScreencast) {
        delete m_portalScreencast;
        m_portalScreencast = nullptr;
    }
    if (m_pipewire) {
        m_pipewire->stop();
        delete m_pipewire;
        m_pipewire = nullptr;
    }

    initFallbackBackend();

    if (m_screenCapture) {
        m_screenCapture->init();
        m_screenCapture->start();
    }

    emit captureFinished();
    emit captureRestarted();
}
#endif

void CaptureController::initFallbackBackend()
{
    if (m_screenCapture) return; // already initialized

    m_screenCapture = new ScreenCast();
    m_screenCapture->setIsCaptureDesktop(m_isCaptureDesktop);
    if (!m_framerate.isEmpty()) m_screenCapture->setCurrentFramerate(m_framerate);
    if (m_isCaptureDesktop) {
        m_screenCapture->setCurrentDisplayIndex(m_currentDisplay);
    } else {
        m_screenCapture->setCurrentWindow(m_currentWindow);
    }
    connect(m_screenCapture, &ScreenCast::currentFrameBuffer,
            this, &CaptureController::frameBufferReady);

    m_screenCastWindow = new ScreenCastWindow(m_screenCapture);
}
