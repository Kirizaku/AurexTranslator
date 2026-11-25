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

#include "overlaycaptureserver.h"
#include "src/utils/overlay_protocol.h"
#include "src/utils/logger.h"

#include <cerrno>
#include <cstddef>
#include <cstring>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <array>
#endif

OverlayCaptureServer::OverlayCaptureServer(QObject *parent)
    : QObject(parent) {}

OverlayCaptureServer::~OverlayCaptureServer()
{
    stop();
}

void OverlayCaptureServer::setActive(bool active)
{
    m_active = active;
    m_controlDirty = true;
}

void OverlayCaptureServer::setFrameCaptureAllowed(bool allowed)
{
    m_allowFrameCapture = allowed;
    m_controlDirty = true;
}

void OverlayCaptureServer::setText(const QString &source, const QString &text, bool visible)
{
    auto clampUtf8 = [](QByteArray &b, int max) {
        if (b.size() <= max) return;
        int n = max;
        while (n > 0 && (static_cast<unsigned char>(b[n]) & 0xC0) == 0x80) --n;
        b.truncate(n);
    };

    {
        QMutexLocker lk(&m_textLock);
        m_textSource = source.toUtf8();
        m_textBody   = text.toUtf8();
        clampUtf8(m_textSource, CAPTURE_TEXT_SRC_MAX);
        clampUtf8(m_textBody,   CAPTURE_TEXT_TEXT_MAX);
        m_textVisible = visible;
    }
    m_textDirty = true;
}

void OverlayCaptureServer::setStyle(const QColor &textColor, const QColor &bgColor, float fontSize,
                                    float zoneX, float zoneY, float zoneW, float zoneH, bool fontSizeAuto,
                                    int cornerRadius, Qt::Alignment align,
                                    int marginTop, int marginBottom, int marginLeft, int marginRight)
{
    {
        QMutexLocker lk(&m_styleLock);
        m_textColor = textColor;
        m_bgColor = bgColor;
        m_fontSize = fontSize;
        m_zoneX = zoneX; m_zoneY = zoneY; m_zoneW = zoneW; m_zoneH = zoneH;
        m_fontSizeAuto = fontSizeAuto;
        m_cornerRadius = cornerRadius;
        m_align = align;
        m_marginTop = marginTop; m_marginBottom = marginBottom;
        m_marginLeft = marginLeft; m_marginRight = marginRight;
    }
    m_styleDirty = true;
}

void OverlayCaptureServer::setFontPath(const QString &path)
{
    {
        QMutexLocker lk(&m_fontLock);
        m_fontPath = path.toUtf8().left(CAPTURE_FONT_PATH_MAX);
    }
    m_fontDirty = true;
}

void OverlayCaptureServer::setActiveClient(const QString &exe, quint32 pid)
{
    {
        QMutexLocker lk(&m_switchLock);
        m_pendingActiveExe = exe;
        m_pendingActivePid = pid;
    }
    m_switchRequested = true;
}

void OverlayCaptureServer::stop()
{
    m_running = false;
}

// ===============================================================
// Wire messages
// ===============================================================

OverlayCaptureServer::ClientSession OverlayCaptureServer::sessionFromHello(qintptr conn, const uint8_t *data, size_t size)
{
    ClientSession session{conn, QString(), 0};
    if (size >= CAPTURE_CLIENT_DATA_SIZE && data[0] == CAPTURE_CLIENT_DATA_TYPE) {
        capture_client_data hello;
        std::memcpy(&hello, data, sizeof(hello));
        char exe[sizeof(hello.exe) + 1] = {0};
        std::memcpy(exe, hello.exe, sizeof(hello.exe));
        session.exe = QString::fromUtf8(exe);
        session.pid = hello.pid;
    }
    return session;
}

void OverlayCaptureServer::closeConnection(qintptr conn)
{
#ifdef Q_OS_WIN
    CloseHandle((HANDLE)conn);
#elif defined(Q_OS_LINUX)
    close((int)conn);
#endif
}

void OverlayCaptureServer::enqueue(int index, MessageKind kind, QByteArray data)
{
    ClientSession &session = m_sessions[index];
    auto it = session.outbox.begin();
    if (it != session.outbox.end() && session.frontOffset > 0) ++it;

    while (it != session.outbox.end()) {
        if (it->kind == kind) it = session.outbox.erase(it);
        else ++it;
    }
    session.outbox.push_back({kind, std::move(data)});
    flush(session);
}

void OverlayCaptureServer::flush(ClientSession &session)
{
    while (!session.dead && !session.outbox.empty()) {
        const QByteArray &data = session.outbox.front().data;
        const char *ptr = data.constData() + session.frontOffset;
        const qsizetype left = data.size() - session.frontOffset;
#ifdef Q_OS_WIN
        DWORD wrote = 0;
        if (!WriteFile((HANDLE)session.conn, ptr, (DWORD)left, &wrote, nullptr)) {
            session.dead = true;
            return;
        }
        if (wrote == 0) return;
#elif defined(Q_OS_LINUX)
        const ssize_t wrote = send((int)session.conn, ptr, (size_t)left, MSG_NOSIGNAL);
        if (wrote < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) session.dead = true;
            return;
        }
#else
        return;
#endif
        session.frontOffset += (qsizetype)wrote;
        if (session.frontOffset < data.size()) return;
        session.outbox.pop_front();
        session.frontOffset = 0;
    }
}

void OverlayCaptureServer::sendControl(int index, bool capturing, bool allowFrameCapture)
{
    capture_control_data msg{};
    msg.capturing = capturing ? 1 : 0;
    msg.allow_frame_capture = allowFrameCapture ? 1 : 0;
    if (!allowFrameCapture)
        m_sessions[index].gpuFallbackRequested = false;
#ifdef Q_OS_LINUX
    msg.device_uuid_valid = m_gpuDeviceUuidValid ? 1 : 0;
    std::memcpy(msg.device_uuid, m_gpuDeviceUuid, sizeof(msg.device_uuid));
#endif
    enqueue(index, MessageKind::Control, QByteArray((const char*)&msg, sizeof(msg)));
}

void OverlayCaptureServer::sendText(int index)
{
    QByteArray src, body;
    bool visible;
    {
        QMutexLocker lk(&m_textLock);
        src = m_textSource;
        body = m_textBody;
        visible = m_textVisible;
    }

    capture_text_data hdr{};
    hdr.type = CAPTURE_TEXT_DATA_TYPE;
    hdr.visible = visible ? 1 : 0;
    hdr.src_len = (uint32_t)src.size();
    hdr.text_len = (uint32_t)body.size();

    QByteArray buf((const char*)&hdr, CAPTURE_TEXT_DATA_HEADER_SIZE);
    buf.append(src);
    buf.append(body);
    enqueue(index, MessageKind::Text, std::move(buf));
}

void OverlayCaptureServer::sendHideText(int index)
{
    capture_text_data hdr{};
    hdr.type = CAPTURE_TEXT_DATA_TYPE;
    enqueue(index, MessageKind::Text, QByteArray((const char*)&hdr, CAPTURE_TEXT_DATA_HEADER_SIZE));
}

void OverlayCaptureServer::sendStyle(int index)
{
    auto toRgba = [](const QColor &c) -> uint32_t {
        return (uint32_t(c.red()) << 24) | (uint32_t(c.green()) << 16) | (uint32_t(c.blue()) << 8) | uint32_t(c.alpha());
    };

    capture_style_data msg{};
    msg.type = CAPTURE_STYLE_DATA_TYPE;
    {
        QMutexLocker lk(&m_styleLock);
        msg.align = (m_align & Qt::AlignRight) ? 2 : (m_align & Qt::AlignHCenter) ? 1 : 0;
        msg.text_color = toRgba(m_textColor);
        msg.bg_color = toRgba(m_bgColor);
        msg.font_size = m_fontSize;
        msg.zone_x = m_zoneX; msg.zone_y = m_zoneY; msg.zone_w = m_zoneW; msg.zone_h = m_zoneH;
        msg.font_size_auto = m_fontSizeAuto ? 1 : 0;
        msg.corner_radius = (uint8_t)qBound(0, m_cornerRadius, 64);
        msg.margin_top = (uint8_t)qBound(0, m_marginTop, 64);
        msg.margin_bottom = (uint8_t)qBound(0, m_marginBottom, 64);
        msg.margin_left = (uint8_t)qBound(0, m_marginLeft, 64);
        msg.margin_right = (uint8_t)qBound(0, m_marginRight, 64);
    }
    enqueue(index, MessageKind::Style, QByteArray((const char*)&msg, sizeof(msg)));
}

void OverlayCaptureServer::sendFont(int index)
{
    QByteArray path;
    {
        QMutexLocker lk(&m_fontLock);
        path = m_fontPath;
    }

    capture_font_data hdr{};
    hdr.type = CAPTURE_FONT_DATA_TYPE;
    hdr.path_len = (uint32_t)path.size();

    QByteArray buf((const char*)&hdr, CAPTURE_FONT_DATA_HEADER_SIZE);
    buf.append(path);
    enqueue(index, MessageKind::Font, std::move(buf));
}

// ===============================================================
// Sessions (server thread)
// ===============================================================

void OverlayCaptureServer::addSession(const ClientSession &session)
{
    Log(Logger::Level::Info, QString("[Overlay] Game connected: %1").arg(session.exe));

    for (int i = 0; i < (int)m_sessions.size(); ++i) {
        if (m_sessions[i].exe == session.exe && m_sessions[i].pid == session.pid) {
#ifdef Q_OS_LINUX
            releaseGpuTexture(m_sessions[i]);
#endif
            closeConnection(m_sessions[i].conn);
            m_sessions[i].conn = session.conn;
            m_sessions[i].outbox.clear();
            m_sessions[i].frontOffset = 0;
            m_sessions[i].dead = false;
            m_sessions[i].gpuFallbackRequested = false;
            if (i == m_activeIndex) {
                pushAllTo(i);
            }
            emitClientList();
            return;
        }
    }

    m_sessions.push_back(session);
    applySwitchRequest(true);
    emitClientList();
}

void OverlayCaptureServer::removeSession(int index)
{
    Log(Logger::Level::Info, QString("[Overlay] Game disconnected: %1").arg(m_sessions[index].exe));
#ifdef Q_OS_LINUX
    releaseGpuTexture(m_sessions[index]);
#endif
    closeConnection(m_sessions[index].conn);
    m_sessions.erase(m_sessions.begin() + index);

    if (index == m_activeIndex) {
        m_activeIndex = -1;
        emit activeClientChanged(QString(), 0);
    } else if (index < m_activeIndex) {
        --m_activeIndex;
    }
    applySwitchRequest(true);
    emitClientList();
}

void OverlayCaptureServer::activate(int index)
{
    if (index == m_activeIndex) return;

    if (m_activeIndex >= 0) {
        sendHideText(m_activeIndex);
        sendControl(m_activeIndex, false, false);
    }

    m_activeIndex = index;
    if (index < 0) {
        emit activeClientChanged(QString(), 0);
        return;
    }
    pushAllTo(index);
    emit activeClientChanged(m_sessions[index].exe, m_sessions[index].pid);
}

void OverlayCaptureServer::pushAllTo(int index)
{
    sendControl(index, m_active, m_allowFrameCapture);
    sendStyle(index);
    sendFont(index);
    sendText(index);
    requestForcedCpuFallback(index);
}

void OverlayCaptureServer::applySwitchRequest(bool sessionsChanged)
{
    if (!m_switchRequested.exchange(false) && !sessionsChanged) return;

    QString target;
    quint32 targetPid = 0;
    {
        QMutexLocker lk(&m_switchLock);
        target = m_pendingActiveExe;
        targetPid = m_pendingActivePid;
    }

#ifdef Q_OS_WIN
    constexpr auto nameCase = Qt::CaseInsensitive;
#else
    constexpr auto nameCase = Qt::CaseSensitive;
#endif
    int matchingGame = -1;
    if (!target.isEmpty()) {
        for (int i = 0; i < (int)m_sessions.size(); ++i) {
            if (m_sessions[i].exe.compare(target, nameCase) != 0) continue;
            if (targetPid == 0 || m_sessions[i].pid == targetPid) {
                activate(i);
                return;
            }
            if (matchingGame < 0) matchingGame = i;
        }
    }
    activate(matchingGame);
}

void OverlayCaptureServer::pushDirtyState()
{
    if (m_activeIndex < 0) return;

    if (m_controlDirty.exchange(false)) {
        sendControl(m_activeIndex, m_active, m_allowFrameCapture);
        requestForcedCpuFallback(m_activeIndex);
    }
    if (m_styleDirty.exchange(false)) sendStyle(m_activeIndex);
    if (m_fontDirty.exchange(false)) sendFont(m_activeIndex);
    if (m_textDirty.exchange(false)) sendText(m_activeIndex);
}

void OverlayCaptureServer::closeAllSessions()
{
    if (m_sessions.empty()) return;

    for (ClientSession &s : m_sessions) {
#ifdef Q_OS_LINUX
        releaseGpuTexture(s);
#endif
        closeConnection(s.conn);
    }
    m_sessions.clear();
    m_activeIndex = -1;
    emit clientListChanged({}, {});
    emit activeClientChanged(QString(), 0);
}

void OverlayCaptureServer::emitClientList()
{
    QStringList names;
    QList<quint32> pids;
    for (const ClientSession &s : m_sessions) { names << s.exe; pids << s.pid; }
    emit clientListChanged(names, pids);
}

void OverlayCaptureServer::requestGpuFallback(int index)
{
    if (index < 0 || index >= (int)m_sessions.size()) return;
    ClientSession &session = m_sessions[index];
    if (session.gpuFallbackRequested) return;

    const uint8_t type = CAPTURE_GPU_FALLBACK_DATA_TYPE;
    session.gpuFallbackRequested = true;
    enqueue(index, MessageKind::GpuFallback, QByteArray((const char *)&type, 1));
#ifdef Q_OS_LINUX
    for (int &fd : session.gpuFds) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
#endif
    Log(Logger::Level::Info,
        QString("[Overlay] Requested CPU readback for %1").arg(session.exe));
}

void OverlayCaptureServer::requestForcedCpuFallback(int index)
{
    if (!m_allowFrameCapture ||
        qEnvironmentVariableIntValue("AT_OVERLAY_FORCE_CPU_CAPTURE") == 0)
        return;

    requestGpuFallback(index);
}

// ===============================================================
// Platform transport
// ===============================================================

#ifdef Q_OS_WIN

void OverlayCaptureServer::winListenerLoop()
{
    while (m_running) {
        HANDLE pipe = CreateNamedPipeA(
            AT_OVERLAY_CAPTURE_PIPE,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Log(Logger::Level::Warning, QString("[Overlay] CreateNamedPipe failed: %1").arg(GetLastError()));
            Sleep(500);
            continue;
        }

        const bool connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!m_running) { CloseHandle(pipe); break; }
        if (!connected) { CloseHandle(pipe); continue; }

        uint8_t buf[256];
        DWORD got = 0;
        if (!ReadFile(pipe, buf, sizeof(buf), &got, nullptr) || got < 1) {
            CloseHandle(pipe);
            continue;
        }

        DWORD mode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
        SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

        QMutexLocker lk(&m_pendingMutex);
        m_pendingSessions.push_back(sessionFromHello((qintptr)pipe, buf, got));
    }
}

void OverlayCaptureServer::unblockListener()
{
    HANDLE h = CreateFileA(AT_OVERLAY_CAPTURE_PIPE, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

void OverlayCaptureServer::start()
{
    if (m_running.exchange(true)) return;

    Log(Logger::Level::Info, QString("[Overlay] Capture server listening on %1").arg(AT_OVERLAY_CAPTURE_PIPE));
    m_winListener = std::thread(&OverlayCaptureServer::winListenerLoop, this);

    uint8_t buf[256];
    while (m_running) {
        std::vector<ClientSession> arrived;
        {
            QMutexLocker lk(&m_pendingMutex);
            arrived.swap(m_pendingSessions);
        }
        for (const ClientSession &s : arrived) addSession(s);

        applySwitchRequest();

        for (int i = (int)m_sessions.size() - 1; i >= 0; --i) {
            const HANDLE h = (HANDLE)m_sessions[i].conn;
            DWORD avail = 0, got = 0;
            flush(m_sessions[i]);
            bool dead = m_sessions[i].dead || !PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr);
            if (!dead && avail > 0 && !ReadFile(h, buf, sizeof(buf), &got, nullptr)) dead = true;
            if (dead) removeSession(i);
        }

        pushDirtyState();
        Sleep(30);
    }

    unblockListener();
    if (m_winListener.joinable()) m_winListener.join();

    {
        QMutexLocker lk(&m_pendingMutex);
        for (const ClientSession &s : m_pendingSessions) closeConnection(s.conn);
        m_pendingSessions.clear();
    }
    closeAllSessions();
    Log(Logger::Level::Info, "[Overlay] Capture server stopped");
}

#elif defined(Q_OS_LINUX)

namespace {

bool bindAbstractSocket(int sock)
{
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    const char *name = AT_OVERLAY_CAPTURE_SOCKET;
    const size_t nameLen = std::strlen(name);
    addr.sun_path[0] = '\0';
    std::memcpy(&addr.sun_path[1], name, nameLen);
    const socklen_t addrLen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + nameLen);
    return ::bind(sock, (struct sockaddr *)&addr, addrLen) == 0;
}

} // namespace

bool OverlayCaptureServer::acceptClient(int listenFd)
{
    const int clientFd = accept4(listenFd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (clientFd < 0) return false;

    uint8_t buf[CAPTURE_CLIENT_DATA_SIZE];
    size_t received = 0;
    for (int tries = 0; tries < 20 && m_running; ++tries) {
        struct pollfd pfd{clientFd, POLLIN, 0};
        if (poll(&pfd, 1, 100) <= 0) continue;
        const ssize_t n = recv(clientFd, buf + received, sizeof(buf) - received, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        if (n <= 0) break;
        received += (size_t)n;
        if (received == sizeof(buf)) {
            addSession(sessionFromHello((qintptr)clientFd, buf, received));
            return true;
        }
    }
    close(clientFd);
    return false;
}

void OverlayCaptureServer::releaseGpuTexture(ClientSession &session)
{
    session.gpuFramePending = false;
    for (int &fd : session.gpuFds) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
    session.gpuTexture = {};
    for (int &fd : session.incomingGpuFds) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
    session.incomingGpuFdCount = 0;
    session.incomingGpuTexture.clear();
}

bool OverlayCaptureServer::receiveClientData(ClientSession &session)
{
    uint8_t buf[CAPTURE_GPU_TEXTURE_DATA_SIZE];
    char control[CMSG_SPACE(sizeof(int) * 4)] = {};
    const size_t remaining = session.incomingGpuTexture.isEmpty() ? 1
                                                                  : CAPTURE_GPU_TEXTURE_DATA_SIZE
                                                                  - (size_t)session.incomingGpuTexture.size();
    struct iovec io{buf, remaining};
    struct msghdr msg{};
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    const ssize_t n = recvmsg((int)session.conn, &msg, MSG_CMSG_CLOEXEC);
    if (n == 0) return false;
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;

    for (cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) continue;
        if (cmsg->cmsg_len < CMSG_LEN(0)) return false;
        const int count = (int)((cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int));
        const int *fds = reinterpret_cast<const int *>(CMSG_DATA(cmsg));
        for (int i = 0; i < count; ++i) {
            if (session.incomingGpuFdCount < 4)
                session.incomingGpuFds[session.incomingGpuFdCount++] = fds[i];
            else close(fds[i]);
        }
    }
    if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) return false;

    if (session.incomingGpuTexture.isEmpty() && buf[0] == CAPTURE_GPU_READY_DATA_TYPE) {
        if (session.incomingGpuFdCount != 0) return false;
        session.gpuFramePending = m_activeIndex >= 0 &&
            &session == &m_sessions[m_activeIndex] && session.gpuTexture.nfd != 0;
        return true;
    }
    session.incomingGpuTexture.append(reinterpret_cast<const char *>(buf), n);
    if ((uint8_t)session.incomingGpuTexture[0] != CAPTURE_GPU_TEXTURE_DATA_TYPE) return false;
    if (session.incomingGpuTexture.size() < CAPTURE_GPU_TEXTURE_DATA_SIZE) return true;

    capture_gpu_texture_data texture{};
    std::memcpy(&texture, session.incomingGpuTexture.constData(), sizeof(texture));
    if (texture.nfd < 1 || texture.nfd > 4 || session.incomingGpuFdCount != texture.nfd ||
        texture.width <= 0 || texture.height <= 0 ||
        texture.width > AT_OVERLAY_FRAME_MAX_WIDTH ||
        texture.height > AT_OVERLAY_FRAME_MAX_HEIGHT) {
        return false;
    }

    for (int &fd : session.gpuFds) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
    session.gpuFramePending = false;
    session.gpuTexture = texture;
    session.gpuTextureGeneration = ++m_nextGpuTextureGeneration;
    for (int i = 0; i < session.incomingGpuFdCount; ++i) {
        session.gpuFds[i] = session.incomingGpuFds[i];
        session.incomingGpuFds[i] = -1;
    }
    session.incomingGpuFdCount = 0;
    session.incomingGpuTexture.clear();
    return true;
}

void OverlayCaptureServer::processGpuFrames()
{
    static bool initialized = false;
    static EGLDisplay display = EGL_NO_DISPLAY;
    static EGLContext context = EGL_NO_CONTEXT;
    static EGLSurface surface = EGL_NO_SURFACE;
    static EGLImageKHR image = EGL_NO_IMAGE_KHR;
    static GLuint texture = 0, framebuffer = 0;
    static GLuint readbackFramebuffer = 0, readbackBuffer = 0;
    static int shmFd = -1;
    static void *mapping = MAP_FAILED;
    static sem_t *captureReady = SEM_FAILED;
    static auto destroyImage = (PFNEGLDESTROYIMAGEKHRPROC)nullptr;
    static bool importFailureLogged = false;
    static uint64_t importedGeneration = 0;
    static bool importerReady = false;
    static uint8_t importerDeviceUuid[16] = {};
    static bool importerDeviceUuidValid = false;

    if (!initialized) {
        initialized = true;
        captureReady = sem_open(AT_OVERLAY_CAPTURE_READY_SEM, O_CREAT, 0600, 0);
        shmFd = shm_open(AT_OVERLAY_FRAME_MAPPING_POSIX, O_CREAT | O_RDWR, 0600);
        if (shmFd >= 0 && ftruncate(shmFd, (off_t)AT_OVERLAY_FRAME_MAPPING_SIZE) == 0)
            mapping = mmap(nullptr, (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE,
                           PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    }

    if (captureReady == SEM_FAILED || mapping == MAP_FAILED) {
        requestGpuFallback(m_activeIndex);
        return;
    }

    auto releaseImporter = [&]() {
        if (display == EGL_NO_DISPLAY) return;
        if (image != EGL_NO_IMAGE_KHR && destroyImage)
            destroyImage(display, image);
        image = EGL_NO_IMAGE_KHR;
        if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
            if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
            if (readbackFramebuffer) glDeleteFramebuffers(1, &readbackFramebuffer);
            if (readbackBuffer) glDeleteRenderbuffers(1, &readbackBuffer);
            if (texture) glDeleteTextures(1, &texture);
        }
        framebuffer = texture = readbackFramebuffer = readbackBuffer = 0;
        if (eglGetCurrentContext() != EGL_NO_CONTEXT)
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
        destroyImage = nullptr;
        importerReady = false;
        importerDeviceUuidValid = false;
        importedGeneration = 0;
    };

    auto initImporter = [&](const uint8_t requestedUuid[16], bool uuidValid) {
        if (importerReady && uuidValid && importerDeviceUuidValid &&
            std::memcmp(importerDeviceUuid, requestedUuid, 16) == 0)
            return true;
        if (importerReady && !uuidValid && !importerDeviceUuidValid)
            return true;
        releaseImporter();

        EGLDisplay selectedDisplay = EGL_NO_DISPLAY;
        bool selectedDevice = false;
        if (uuidValid) {
            auto queryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
                eglGetProcAddress("eglQueryDevicesEXT"));
            auto queryDeviceBinary = reinterpret_cast<PFNEGLQUERYDEVICEBINARYEXTPROC>(
                eglGetProcAddress("eglQueryDeviceBinaryEXT"));
            auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
                eglGetProcAddress("eglGetPlatformDisplayEXT"));
            if (queryDevices && queryDeviceBinary && getPlatformDisplay) {
                EGLint deviceCount = 0;
                if (queryDevices(0, nullptr, &deviceCount) && deviceCount > 0) {
                    std::vector<EGLDeviceEXT> devices((size_t)deviceCount);
                    if (queryDevices(deviceCount, devices.data(), &deviceCount)) {
                        for (EGLint i = 0; i < deviceCount; ++i) {
                            uint8_t deviceUuid[16] = {};
                            EGLint uuidSize = 0;
                            if (queryDeviceBinary(devices[(size_t)i], EGL_DEVICE_UUID_EXT,
                                                  sizeof(deviceUuid), deviceUuid, &uuidSize) &&
                                uuidSize == (EGLint)sizeof(deviceUuid) &&
                                std::memcmp(deviceUuid, requestedUuid, sizeof(deviceUuid)) == 0) {
                                selectedDisplay = getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT,
                                                                      devices[(size_t)i], nullptr);
                                selectedDevice = selectedDisplay != EGL_NO_DISPLAY;
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (selectedDisplay == EGL_NO_DISPLAY) {
            selectedDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        }
        if (uuidValid) {
            Log(Logger::Level::Info, selectedDevice
                ? QStringLiteral("[Overlay] Selected EGL device matching the Vulkan UUID")
                : QStringLiteral("[Overlay] No EGL device matched Vulkan UUID; trying default EGL display"));
        }

        EGLint major = 0, minor = 0;
        if (selectedDisplay == EGL_NO_DISPLAY || !eglInitialize(selectedDisplay, &major, &minor))
            return false;
        display = selectedDisplay;
        destroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
        const EGLint configAttrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
        EGLConfig config{};
        EGLint count = 0;
        if (!eglChooseConfig(display, configAttrs, &config, 1, &count) || count == 0 ||
            !eglBindAPI(EGL_OPENGL_API)) {
            releaseImporter();
            return false;
        }
        const EGLint pbufferAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, pbufferAttrs);
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
        if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE ||
            !eglMakeCurrent(display, surface, surface, context)) {
            releaseImporter();
            return false;
        }
        const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        auto getUuid = reinterpret_cast<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(eglGetProcAddress("glGetUnsignedBytei_vEXT"));
        m_gpuDeviceUuidValid = false;
        std::memset(m_gpuDeviceUuid, 0, sizeof(m_gpuDeviceUuid));
        if (extensions && std::strstr(extensions, "GL_EXT_memory_object") && getUuid) {
            getUuid(GL_DEVICE_UUID_EXT, 0, m_gpuDeviceUuid);
            m_gpuDeviceUuidValid = glGetError() == GL_NO_ERROR;
            if (m_gpuDeviceUuidValid) {
                const QByteArray uuid(reinterpret_cast<const char *>(m_gpuDeviceUuid), 16);
                Log(Logger::Level::Info, QString("[Overlay] EGL importer GPU UUID: %1 (%2)")
                    .arg(QString::fromLatin1(uuid.toHex()))
                    .arg(selectedDevice ? QStringLiteral("matched Vulkan device")
                                        : QStringLiteral("default display fallback")));
                if (m_activeIndex >= 0)
                    sendControl(m_activeIndex, m_active, m_allowFrameCapture);
            }
        }
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGenFramebuffers(1, &framebuffer);
        glGenFramebuffers(1, &readbackFramebuffer);
        glGenRenderbuffers(1, &readbackBuffer);
        if (!texture || !framebuffer || !readbackFramebuffer || !readbackBuffer || !destroyImage ||
            !eglGetProcAddress("eglCreateImageKHR")) {
            releaseImporter();
            return false;
        }
        if (uuidValid) {
            std::memcpy(importerDeviceUuid, requestedUuid, 16);
            importerDeviceUuidValid = true;
        }
        importerReady = true;
        return true;
    };

    if (m_activeIndex < 0) return;
    ClientSession &session = m_sessions[m_activeIndex];
    if (!session.gpuFramePending) return;
    session.gpuFramePending = false;
    if (session.gpuFallbackRequested) return;

    const auto &td = session.gpuTexture;
    if (td.nfd == 0) return;
    const bool frameUuidValid = std::any_of(std::begin(td.device_uuid), std::end(td.device_uuid), [](uint8_t byte) { return byte != 0; });
    if (!initImporter(td.device_uuid, frameUuidValid)) {
        requestGpuFallback(m_activeIndex);
        Log(Logger::Level::Warning, QStringLiteral("[Overlay] Could not initialize EGL importer for Vulkan GPU"));
        return;
    }
    const bool packed32 = td.format == 0x34325241 /* AR24 */ ||
                          td.format == 0x34325258 /* XR24 */ ||
                          td.format == 0x34324241 /* AB24 */ ||
                          td.format == 0x34324258 /* XB24 */ ||
                          td.format == 0x30335241 /* AR30 */ ||
                          td.format == 0x30335258 /* XR30 */ ||
                          td.format == 0x30334241 /* AB30 */ ||
                          td.format == 0x30334258 /* XB30 */;
    bool validPlanes = td.nfd >= 1 && td.nfd <= 4;
    for (int i = 0; validPlanes && i < td.nfd; ++i)
        validPlanes = session.gpuFds[i] >= 0 && td.strides[i] > 0;
    if (td.width <= 0 || td.height <= 0 || !validPlanes || !packed32) {
        requestGpuFallback(m_activeIndex);
        if (!importFailureLogged) {
            Log(Logger::Level::Warning, QString("[Overlay] Unsupported GPU frame: format 0x%1, %2 plane(s)")
                    .arg((uint32_t)td.format, 8, 16, QLatin1Char('0')).arg(td.nfd));
            importFailureLogged = true;
        }
        return;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        requestGpuFallback(m_activeIndex);
        if (!importFailureLogged) {
            Log(Logger::Level::Warning, QString("[Overlay] Cannot activate EGL importer context: 0x%1")
                    .arg(eglGetError(), 0, 16));
            importFailureLogged = true;
        }
        return;
    }
    if (importedGeneration != session.gpuTextureGeneration) {
        if (image != EGL_NO_IMAGE_KHR && destroyImage) {
            destroyImage(display, image);
            image = EGL_NO_IMAGE_KHR;
        }
        std::vector<EGLint> attrs = {
            EGL_WIDTH, td.width, EGL_HEIGHT, td.height,
            EGL_LINUX_DRM_FOURCC_EXT, td.format};
        const EGLint planeFdAttrs[] = {
            EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
            EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT};
        const EGLint planeOffsetAttrs[] = {
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
            EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT};
        const EGLint planePitchAttrs[] = {
            EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
            EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT};
        const EGLint planeModifierLoAttrs[] = {
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
            EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT};
        const EGLint planeModifierHiAttrs[] = {
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
            EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT};
        for (int i = 0; i < td.nfd; ++i) {
            attrs.insert(attrs.end(), {planeFdAttrs[i], session.gpuFds[i],
                planeOffsetAttrs[i], td.offsets[i],
                planePitchAttrs[i], td.strides[i]});
            if (td.modifier != 0x00ffffffffffffffULL) {
                attrs.insert(attrs.end(), {planeModifierLoAttrs[i],
                    (EGLint)(td.modifier & 0xffffffffu),
                    planeModifierHiAttrs[i], (EGLint)(td.modifier >> 32)});
            }
        }
        attrs.push_back(EGL_NONE);
        auto createImage = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        image = createImage(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());
        if (image == EGL_NO_IMAGE_KHR) {
            const EGLint importError = eglGetError();
            requestGpuFallback(m_activeIndex);
            if (!importFailureLogged) {
                Log(Logger::Level::Warning, QString("[Overlay] EGL DMA-BUF import failed: 0x%1, format 0x%2, modifier 0x%3")
                        .arg(importError, 0, 16)
                        .arg((uint32_t)td.format, 8, 16, QLatin1Char('0'))
                        .arg(td.modifier, 16, 16, QLatin1Char('0')));
                importFailureLogged = true;
            }
            return;
        }

        auto imageTarget = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        if (!imageTarget) {
            requestGpuFallback(m_activeIndex);
            return;
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        imageTarget(GL_TEXTURE_2D, image);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        const GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
            requestGpuFallback(m_activeIndex);
            if (!importFailureLogged) {
                Log(Logger::Level::Warning, QString("[Overlay] Imported DMA-BUF framebuffer is incomplete: 0x%1")
                        .arg(framebufferStatus, 0, 16));
                importFailureLogged = true;
            }
            return;
        }
        glBindRenderbuffer(GL_RENDERBUFFER, readbackBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, td.width, td.height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, readbackFramebuffer);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, readbackBuffer);
        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Log(Logger::Level::Warning, QStringLiteral("[Overlay] GPU readback framebuffer is incomplete"));
            requestGpuFallback(m_activeIndex);
            return;
        }
        importedGeneration = session.gpuTextureGeneration;
    }

    auto *hdr = (at_overlay_frame_header *)mapping;
    uint8_t *dst = (uint8_t *)mapping + AT_OVERLAY_FRAME_HEADER_SIZE;
    auto *sequence = reinterpret_cast<uint32_t *>(
        static_cast<uint8_t *>(mapping) + offsetof(at_overlay_frame_header, reserved));
    uint32_t seq = __atomic_load_n(sequence, __ATOMIC_RELAXED);
    if (seq & 1u) ++seq;
    __atomic_store_n(sequence, seq + 1u, __ATOMIC_SEQ_CST);

    const int stride = td.width * 4;
    (void)glGetError();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, readbackFramebuffer);
    glBlitFramebuffer(0, 0, td.width, td.height, 0, 0, td.width, td.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readbackFramebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, td.width, td.height, GL_BGRA, GL_UNSIGNED_BYTE, dst);
    const GLenum readError = glGetError();
    if (readError != GL_NO_ERROR) {
        __atomic_store_n(sequence, seq + 2u, __ATOMIC_RELEASE);
        requestGpuFallback(m_activeIndex);
        Log(Logger::Level::Warning, QString("[Overlay] GPU frame readback failed: 0x%1")
            .arg(readError, 0, 16));
        return;
    }
    if (td.flip) {
        for (int y = 0; y < td.height / 2; ++y)
            for (int x = 0; x < stride; ++x)
                std::swap(dst[(size_t)y * stride + x], dst[(size_t)(td.height - 1 - y) * stride + x]);
    }
    hdr->width = td.width;
    hdr->height = td.height;
    hdr->stride = stride;
    hdr->format = AT_OVERLAY_FRAME_FORMAT_BGRA8;
    ++hdr->frame_counter;
    __atomic_store_n(sequence, seq + 2u, __ATOMIC_RELEASE);
    glFinish();
    sem_post(captureReady);
    importFailureLogged = false;
}

void OverlayCaptureServer::start()
{
    if (m_running.exchange(true)) return;

    Log(Logger::Level::Info, QString("[Overlay] Capture server listening on abstract socket %1").arg(AT_OVERLAY_CAPTURE_SOCKET));

    int listenFd = -1;
    while (m_running && listenFd < 0) {
        listenFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (listenFd < 0) {
            Log(Logger::Level::Warning, QString("[Overlay] socket() failed: %1").arg(strerror(errno)));
        } else if (!bindAbstractSocket(listenFd) || listen(listenFd, 8) != 0) {
            Log(Logger::Level::Warning, QString("[Overlay] bind/listen failed: %1").arg(strerror(errno)));
            close(listenFd);
            listenFd = -1;
        }
        if (listenFd < 0) {
            struct timespec ts{0, 500000000L};
            nanosleep(&ts, nullptr);
        }
    }

    while (m_running && listenFd >= 0) {
        std::vector<struct pollfd> pfds;
        pfds.reserve(m_sessions.size() + 1);
        pfds.push_back({listenFd, POLLIN, 0});
        for (const ClientSession &s : m_sessions) pfds.push_back({(int)s.conn, POLLIN, 0});

        poll(pfds.data(), pfds.size(), 30);

        if ((pfds[0].revents & POLLIN) && acceptClient(listenFd)) continue;

        applySwitchRequest();

        for (int i = (int)m_sessions.size() - 1; i >= 0; --i) {
            const short revents = pfds[i + 1].revents;
            flush(m_sessions[i]);
            bool dead = m_sessions[i].dead || (revents & (POLLHUP | POLLERR)) != 0;
            if (!dead && (revents & POLLIN)) {
                dead = !receiveClientData(m_sessions[i]);
            }
            if (dead) removeSession(i);
        }

        pushDirtyState();
        processGpuFrames();
    }

    closeAllSessions();
    if (listenFd >= 0) close(listenFd);
    Log(Logger::Level::Info, "[Overlay] Capture server stopped");
}

#else

void OverlayCaptureServer::start() {}

#endif
