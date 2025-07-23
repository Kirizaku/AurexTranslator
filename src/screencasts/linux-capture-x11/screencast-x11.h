#ifndef X11_SCREENCAST_H
#define X11_SCREENCAST_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xrandr.h>

#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Status
#undef Unsorted

struct DisplayInfo {
    int x, y;
    unsigned int width, height;
    QString name;
};

struct WindowInfo {
    unsigned long window_id;
    QString title;
};

class ScreenCast : public QThread
{
    Q_OBJECT

public:
    explicit ScreenCast(QObject *parent = nullptr);
    void stop();

    QVector<DisplayInfo> getDisplays();
    QVector<WindowInfo> getWindows();
    void setCurrentDisplayIndex(int index) { m_currentDisplay = index; }
    unsigned long setCurrentWindow(unsigned long value) { m_currentWindow = value; return m_currentWindow; }

    void setIsProcessed(bool value){ m_isProcessed = value; }
    void wakeWaitCondition() { QMutexLocker locker(&m_mutex); m_waitCondition.wakeOne(); }

    void setIsCaptureDesktop(bool value) { m_isCaptureDesktop = value; }
    bool isCaptureDesktop() const { return m_isCaptureDesktop; }

signals:
    void currentFrameBuffer(uint32_t height, uint32_t width, void* data);

public slots:
    void setCurrentFramerate(const QString &framerate) { m_framerate = framerate.toInt(); }

protected:
    void run() override;

private:
    Display *m_x11Display = nullptr;
    XShmSegmentInfo m_shmInfo;
    XImage* m_xShmImage = nullptr;
    XImage* m_xImage = nullptr;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_isProcessed = false;

    bool m_loop = true;
    bool m_stopped = false;
    bool m_isCaptureDesktop = true;
    void captureDesktop(const int currentIndex);
    void captureWindow(const Window &window);

    int m_currentDisplay = -1;
    unsigned long m_currentWindow;
    int m_framerate = 15;

    void cleanup();
};

#endif // X11_SCREENCAST_H
