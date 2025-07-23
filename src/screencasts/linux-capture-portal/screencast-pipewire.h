#ifndef PIPEWIRE_SCREENCAST_H
#define PIPEWIRE_SCREENCAST_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <pipewire/pipewire.h>
#include <spa/param/video/raw.h>
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>

class Pipewire : public QThread
{
    Q_OBJECT
public:
    explicit Pipewire(QObject *parent = nullptr);
    ~Pipewire();

    void init(int pipewire_id);
    void stop();

    bool isStopped() const { return m_stopped; }
    void setIsStopped(bool value) { m_stopped = value; }
    void setIsProcessed(bool value) { QMutexLocker locker(&m_mutex); m_isProcessed = value; }
    void wakeWaitCondition() { QMutexLocker locker(&m_mutex); m_waitCondition.wakeOne(); }

signals:
    void currentFrameBuffer(uint32_t height, uint32_t width, void* data);

public slots:
    void setCurrentFramerate(const QString &framerate) { m_framerate = framerate.toInt(); }

protected:
    void run() override;

private:
    struct spa_video_info format;
    pw_main_loop* loop = nullptr;
    pw_stream* stream = nullptr;
    pw_properties* props = nullptr;

    static void on_stream_state_changed(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* error);
    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);
    static void on_process(void* userdata);

    static const pw_stream_events stream_events;

    int m_framerate = 15;
    bool m_stopped = false;
    bool m_isProcessed = false;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
};

#endif // PIPEWIRE_SCREENCAST_H
