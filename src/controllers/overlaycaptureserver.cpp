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

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
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
            closeConnection(m_sessions[i].conn);
            m_sessions[i].conn = session.conn;
            m_sessions[i].outbox.clear();
            m_sessions[i].frontOffset = 0;
            m_sessions[i].dead = false;
            if (i == m_activeIndex) pushAllTo(i);
            emitClientList();
            return;
        }
    }

    m_sessions.push_back(session);
    if (m_activeIndex < 0) activate((int)m_sessions.size() - 1);
    emitClientList();
}

void OverlayCaptureServer::removeSession(int index)
{
    Log(Logger::Level::Info, QString("[Overlay] Game disconnected: %1").arg(m_sessions[index].exe));
    closeConnection(m_sessions[index].conn);
    m_sessions.erase(m_sessions.begin() + index);

    if (index == m_activeIndex) {
        m_activeIndex = -1;
        if (m_sessions.empty()) {
            emit activeClientChanged(QString(), 0);
        } else {
            activate(0);
        }
    } else if (index < m_activeIndex) {
        --m_activeIndex;
    }
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
}

void OverlayCaptureServer::applySwitchRequest()
{
    if (!m_switchRequested.exchange(false)) return;

    QString target;
    quint32 targetPid = 0;
    {
        QMutexLocker lk(&m_switchLock);
        target = m_pendingActiveExe;
        targetPid = m_pendingActivePid;
    }

    for (int i = 0; i < (int)m_sessions.size(); ++i) {
        if (m_sessions[i].exe == target && (targetPid == 0 || m_sessions[i].pid == targetPid)) {
            activate(i);
            return;
        }
    }
    activate(-1);
}

void OverlayCaptureServer::pushDirtyState()
{
    if (m_activeIndex < 0) return;

    if (m_controlDirty.exchange(false)) sendControl(m_activeIndex, m_active, m_allowFrameCapture);
    if (m_styleDirty.exchange(false)) sendStyle(m_activeIndex);
    if (m_fontDirty.exchange(false)) sendFont(m_activeIndex);
    if (m_textDirty.exchange(false)) sendText(m_activeIndex);
}

void OverlayCaptureServer::closeAllSessions()
{
    if (m_sessions.empty()) return;

    for (const ClientSession &s : m_sessions) closeConnection(s.conn);
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

    uint8_t buf[256];
    for (int tries = 0; tries < 20 && m_running; ++tries) {
        struct pollfd pfd{clientFd, POLLIN, 0};
        if (poll(&pfd, 1, 100) <= 0) continue;
        const ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        addSession(sessionFromHello((qintptr)clientFd, buf, (size_t)n));
        return true;
    }
    close(clientFd);
    return false;
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
                uint8_t buf[256];
                const ssize_t n = recv((int)m_sessions[i].conn, buf, sizeof(buf), 0);
                dead = n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK);
            }
            if (dead) removeSession(i);
        }

        pushDirtyState();
    }

    closeAllSessions();
    if (listenFd >= 0) close(listenFd);
    Log(Logger::Level::Info, "[Overlay] Capture server stopped");
}

#else

void OverlayCaptureServer::start() {}

#endif
