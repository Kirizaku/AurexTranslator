#include "screencast-pipewire.h"
#include "src/utils/logger.h"

Pipewire::Pipewire(QObject *parent)
    : QThread{parent} {}

Pipewire::~Pipewire() {}

void Pipewire::stop()
{
    m_stopped = true;

    if (loop) {
        pw_main_loop_quit(loop);
    }

    m_isProcessed = true;
    m_waitCondition.wakeOne();

    wait();

    if (stream) {
        pw_stream_destroy(stream);
        stream = nullptr;
    }

    if (loop) {
        pw_main_loop_destroy(loop);
        loop = nullptr;
    }
}

void Pipewire::init(int pipewire_id)
{
    pw_init(nullptr, nullptr);

    loop = pw_main_loop_new(nullptr);

    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        nullptr);

    stream = pw_stream_new_simple(
        pw_main_loop_get_loop(loop),
        "video-capture",
        props,
        &stream_events,
        this);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    // Create rectangle objects
    spa_rectangle default_size = SPA_RECTANGLE(320, 240);
    spa_rectangle min_size = SPA_RECTANGLE(1, 1);
    spa_rectangle max_size = SPA_RECTANGLE(4096, 4096);

    // Create fraction objects
    spa_fraction default_framerate = SPA_FRACTION(25, 1);
    spa_fraction min_framerate = SPA_FRACTION(0, 1);
    spa_fraction max_framerate = SPA_FRACTION(1000, 1);

    const spa_pod* param = reinterpret_cast<const spa_pod*>(
        spa_pod_builder_add_object(&b,
                                   SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                                   SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
                                   SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                                   SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(4,
                                                                  SPA_VIDEO_FORMAT_BGRA, /* default */
                                                                  SPA_VIDEO_FORMAT_RGBA,
                                                                  SPA_VIDEO_FORMAT_BGRx,
                                                                  SPA_VIDEO_FORMAT_RGBx),
                                   SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
                                       &default_size, &min_size, &max_size),
                                   SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                                       &default_framerate, &min_framerate, &max_framerate)));

    const spa_pod* params[] = {param};
    pw_stream_flags flags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);

    Log(Logger::Level::Info, "[PipeWire] PipeWire initialized");
    pw_stream_connect(stream,
                      PW_DIRECTION_INPUT,
                      pipewire_id,
                      flags,
                      params, 1);
}

void Pipewire::run()
{
    if (loop) pw_main_loop_run(loop);
}

void Pipewire::on_stream_state_changed(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
    auto self = static_cast<Pipewire*>(userdata);
    Log(Logger::Level::Info, QString("[pipewire] Stream state: %1").arg(pw_stream_state_as_string(state)));

    switch (state) {
    case PW_STREAM_STATE_UNCONNECTED:
        pw_main_loop_quit(self->loop);
        break;
    case PW_STREAM_STATE_PAUSED:
        pw_stream_set_active(self->stream, true);
        break;
    default:
        break;
    }
}

void Pipewire::on_process(void* userdata)
{
    auto self = static_cast<Pipewire*>(userdata);

    int frameTime = 1000 / self->m_framerate;
    msleep(frameTime);

    if (!self->m_stopped) {
        self->m_isProcessed = false;
    }

    pw_buffer* b;
    spa_buffer* buf;

    if ((b = pw_stream_dequeue_buffer(self->stream)) == nullptr) {
        return;
    }

    buf = b->buffer;
    if (buf->datas[0].data == nullptr) {
        return;
    }

    emit self->currentFrameBuffer(self->format.info.raw.size.height, self->format.info.raw.size.width, buf->datas[0].data);
    pw_stream_queue_buffer(self->stream, b);

    QMutexLocker locker(&self->m_mutex);
    while(!self->m_isProcessed) {
        self->m_waitCondition.wait(&self->m_mutex);
    }
}

void Pipewire::on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param)
{
    auto self = static_cast<Pipewire*>(userdata);

    if (param == nullptr || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param, &self->format.media_type, &self->format.media_subtype) < 0)
        return;

    if (self->format.media_type != SPA_MEDIA_TYPE_video ||
        self->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    if (spa_format_video_raw_parse(param, &self->format.info.raw) < 0)
        return;

    Log(Logger::Level::Info, QString("[pipewire] Got video format:"));
    Log(Logger::Level::Info, QString("[pipewire]   Format: %1 (%2)")
                                      .arg(self->format.info.raw.format)
                                      .arg(spa_debug_type_find_name(spa_type_video_format,
                                                                    self->format.info.raw.format)));
    Log(Logger::Level::Info, QString("[pipewire]   Size: %1x%2")
                                      .arg(self->format.info.raw.size.width)
                                      .arg(self->format.info.raw.size.height));
    Log(Logger::Level::Info, QString("[pipewire]   Framerate: %1/%2")
                                      .arg(self->format.info.raw.framerate.num)
                                      .arg(self->format.info.raw.framerate.denom));
}

const pw_stream_events Pipewire::stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_stream_state_changed,
    .param_changed = on_param_changed,
    .process = on_process,
};
