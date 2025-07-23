#ifndef WIN_SCREENCAST_H
#define WIN_SCREENCAST_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <QPixmap>
#include <windows.h>

struct DisplayInfo {
    int x, y;
    unsigned long width, height;
    QString name;
};

struct WindowInfo {
    HWND window_id;
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
    void setCurrentDisplayIndex(int value) { m_currentDisplay = value; }
    void setCurrentWindow(const HWND &value) { m_currentWindow = value; }

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
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_isProcessed = false;

    int m_framerate = 15;
    bool m_loop = true;
    bool m_stopped = false;

    bool m_isCaptureDesktop = true;
    int m_currentDisplay = -1;

    void captureDesktop(const int currentIndex);
    void captureWindow(const HWND &window_id);
    void* pixelData = nullptr;
    HWND m_currentWindow;

    void cleanup();
};

#endif // WIN_SCREENCAST_H
