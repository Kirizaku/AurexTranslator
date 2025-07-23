#include "screencast-x11.h"
#include "src/utils/logger.h"

#include <sys/ipc.h>
#include <sys/shm.h>

ScreenCast::ScreenCast(QObject *parent)
    : QThread{parent}
{
    // X11 Initialization
    m_x11Display = XOpenDisplay(nullptr);
    if (!m_x11Display) {
        Log(Logger::Level::Critical, QString("[X11] Cannot open X display"));
        return;
    }

    // X11 Extension XComposite
    int event_base, error_base;
    if (!XCompositeQueryExtension(m_x11Display, &event_base, &error_base)) {
        Log(Logger::Level::Critical, QString("[X11] XComposite extension not available"));
        return;
    }

    if (!XFixesQueryExtension(m_x11Display, &event_base, &error_base)) {
        Log(Logger::Level::Critical, QString("[X11] XFixes extension not available"));
        return;
    }
}

void ScreenCast::stop()
{
    m_loop = false;
    m_stopped = true;

    m_isProcessed = true;
    m_waitCondition.wakeOne();

    wait();
    cleanup();

    if (m_x11Display) XCloseDisplay(m_x11Display);
}

void ScreenCast::cleanup()
{
    if (m_xShmImage) {
        XShmDetach(m_x11Display, &m_shmInfo);
        XDestroyImage(m_xShmImage);
        if (m_shmInfo.shmaddr != (char*)-1) {
            shmdt(m_shmInfo.shmaddr);
        }
        if (m_shmInfo.shmid != -1) {
            shmctl(m_shmInfo.shmid, IPC_RMID, 0);
        }
        m_xShmImage = nullptr;
    }

    if (m_xImage) {
        XDestroyImage(m_xImage);
        m_xImage = nullptr;
    }
}

QVector<DisplayInfo> ScreenCast::getDisplays()
{
    QVector<DisplayInfo> listDisplays;

    int screen = DefaultScreen(m_x11Display);
    Window root = RootWindow(m_x11Display, screen);

    XRRScreenResources* res = XRRGetScreenResources(m_x11Display, root);
    if (!res) return listDisplays;

    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo* output = XRRGetOutputInfo(m_x11Display, res, res->outputs[i]);
        if (output && output->connection == RR_Connected) {
            XRRCrtcInfo* crtc = XRRGetCrtcInfo(m_x11Display, res, output->crtc);
            if (crtc) {
                listDisplays.append({
                    crtc->x, crtc->y,
                    crtc->width, crtc->height,
                    QString::fromLatin1(output->name)
                });
                XRRFreeCrtcInfo(crtc);
            }
        }
        if (output) XRRFreeOutputInfo(output);
    }

    XRRFreeScreenResources(res);
    return listDisplays;
}

QVector<WindowInfo> ScreenCast::getWindows()
{
    QVector<WindowInfo> listWindows;

    Atom net_client_list = XInternAtom(m_x11Display, "_NET_CLIENT_LIST", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Window* client_list = nullptr;

    if (XGetWindowProperty(m_x11Display, RootWindow(m_x11Display, DefaultScreen(m_x11Display)), net_client_list,
                           0, 0x7FFFFFFFFFFFFFFF, False, XA_WINDOW,
                           &actual_type, &actual_format,
                           &nitems, &bytes_after,
                           (unsigned char**)&client_list) == Success) {

        for (unsigned long i = 0; i < nitems; i++) {
            Window w = client_list[i];
            Atom net_wm_name = XInternAtom(m_x11Display, "_NET_WM_NAME", False);
            Atom utf8_string = XInternAtom(m_x11Display, "UTF8_STRING", False);
            char* name = nullptr;

            if (XGetWindowProperty(m_x11Display, w, net_wm_name,
                                   0, 0x7FFFFFFFFFFFFFFF, False, utf8_string,
                                   &actual_type, &actual_format,
                                   &nitems, &bytes_after,
                                   (unsigned char**)&name) == Success && name) {

                QString title = QString::fromUtf8(name);
                if (!title.isEmpty()) {
                    listWindows.append({
                        w, title
                    });
                }
                XFree(name);
            }
        }
        if (client_list) {
            XFree(client_list);
        }
    }
    return listWindows;
}

void ScreenCast::captureDesktop(const int currentIndex)
{
    QVector<DisplayInfo> listDisplays = getDisplays();

    if (currentIndex != -1 && currentIndex <= listDisplays.size() - 1)
    {
        DisplayInfo monitor = listDisplays[currentIndex];

        if (monitor.width <= 0 || monitor.height <= 0) { return; }

        if (m_xShmImage) { cleanup(); }

        m_xShmImage = XShmCreateImage(m_x11Display,
                                    DefaultVisual(m_x11Display, DefaultScreen(m_x11Display)),
                                    24,
                                    ZPixmap,
                                    nullptr,
                                    &m_shmInfo,
                                    monitor.width,
                                    monitor.height);
        if (!m_xShmImage) {
            Log(Logger::Level::Critical, QString("[X11] XShmCreateImage failed"));
            return;
        }

        m_shmInfo.shmid = shmget(IPC_PRIVATE, m_xShmImage->bytes_per_line * m_xShmImage->height, IPC_CREAT|0777);
        if (m_shmInfo.shmid == -1) {
            Log(Logger::Level::Critical, QString("[X11] Shmget failed"));
            cleanup();
            return;
        }

        m_shmInfo.shmaddr = m_xShmImage->data = (char*)shmat(m_shmInfo.shmid, 0, 0);
        if (m_shmInfo.shmaddr == (char*)-1) {
            Log(Logger::Level::Critical, QString("[X11] Shmat failed"));
            cleanup();
            return;
        }

        m_shmInfo.readOnly = False;
        if (!XShmAttach(m_x11Display, &m_shmInfo)) {
            Log(Logger::Level::Critical, QString("[X11] XShmAttach failed"));
            cleanup();
            return;
        }

        int xshm_major, xshm_minor;
        int xshm_pixmaps;
        if (!XShmQueryVersion(m_x11Display, &xshm_major, &xshm_minor, &xshm_pixmaps)) {
            Log(Logger::Level::Critical, QString("[X11] XShm not available"));
            cleanup();
            return;
        }

        if (!XShmGetImage(m_x11Display,
                          RootWindow(m_x11Display, DefaultScreen(m_x11Display)),
                          m_xShmImage,
                          monitor.x,
                          monitor.y,
                          AllPlanes)) {
            cleanup();
            m_isProcessed = true;
            m_waitCondition.wakeOne();
            return;
        }

        emit currentFrameBuffer(m_xShmImage->height, m_xShmImage->width, m_xShmImage->data);
    }
}

void ScreenCast::captureWindow(const Window &window)
{
    if (m_xImage) cleanup();

    if (!m_x11Display || window == 0)
        return;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(m_x11Display, window, &attrs)) {
        Log(Logger::Level::Critical, QString("[X11] Failed to get window attributes"));
        return;
    }

    XCompositeRedirectWindow(m_x11Display, window, CompositeRedirectAutomatic);
    XSync(m_x11Display, False);

    m_xImage = XGetImage(m_x11Display, window,
                               0, 0,
                               attrs.width, attrs.height,
                               AllPlanes, ZPixmap);
    if (!m_xImage) {
        cleanup();
        m_isProcessed = true;
        m_waitCondition.wakeOne();
        return;
    }

    emit currentFrameBuffer(m_xImage->height, m_xImage->width, m_xImage->data);
}

void ScreenCast::run()
{
    while(m_loop)
    {
        int frameTime = 1000 / m_framerate;
        msleep(frameTime);

        if (!m_stopped) {
            m_isProcessed = false;
        }

        if (m_isCaptureDesktop) {
            captureDesktop(m_currentDisplay);
        }
        else {
            captureWindow(m_currentWindow);
        }

        QMutexLocker locker(&m_mutex);
        while(!m_isProcessed) {
            m_waitCondition.wait(&m_mutex);
        }
    }
    cleanup();
}
