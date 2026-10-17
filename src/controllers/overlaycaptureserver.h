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

#ifndef OVERLAYCAPTURESERVER_H
#define OVERLAYCAPTURESERVER_H

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <QColor>
#include <QStringList>
#include <QtGlobal>
#include <atomic>
#include <deque>
#include <thread>

class OverlayCaptureServer : public QObject
{
    Q_OBJECT
public:
    explicit OverlayCaptureServer(QObject *parent = nullptr);
    ~OverlayCaptureServer() override;

    void setActive(bool active);
    void setFrameCaptureAllowed(bool allowed);
    void setText(const QString &source, const QString &text, bool visible);
    void setStyle(const QColor &textColor, const QColor &bgColor, float fontSize,
                 float zoneX, float zoneY, float zoneW, float zoneH, bool fontSizeAuto,
                 int cornerRadius, Qt::Alignment align,
                 int marginTop, int marginBottom, int marginLeft, int marginRight);
    void setFontPath(const QString &path);
    void setActiveClient(const QString &exe, quint32 pid = 0);

public slots:
    void start();
    void stop();

signals:
    void clientListChanged(const QStringList &exeNames, const QList<quint32> &pids);
    void activeClientChanged(const QString &exe, quint32 pid);

private:
    enum class MessageKind { Control, Style, Font, Text };
    struct Outgoing {
        MessageKind kind;
        QByteArray data;
    };

    struct ClientSession {
        qintptr conn;
        QString exe;
        quint32 pid = 0;
        std::deque<Outgoing> outbox;
        qsizetype frontOffset = 0;
        bool dead = false;
    };

    static ClientSession sessionFromHello(qintptr conn, const uint8_t *data, size_t size);
    static void closeConnection(qintptr conn);
    void enqueue(int index, MessageKind kind, QByteArray data);
    void flush(ClientSession &session);

    void sendControl(int index, bool capturing, bool allowFrameCapture);
    void sendText(int index);
    void sendHideText(int index);
    void sendStyle(int index);
    void sendFont(int index);

    // Server thread only
    void addSession(const ClientSession &session);
    void removeSession(int index);
    void activate(int index);
    void pushAllTo(int index);
    void applySwitchRequest();
    void pushDirtyState();
    void closeAllSessions();
    void emitClientList();

    std::vector<ClientSession> m_sessions;
    int m_activeIndex = -1;

#ifdef Q_OS_WIN
    void winListenerLoop();
    void unblockListener();
    std::thread m_winListener;
    QMutex m_pendingMutex;
    std::vector<ClientSession> m_pendingSessions;
#elif defined(Q_OS_LINUX)
    bool acceptClient(int listenFd);
#endif

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_active{true};
    std::atomic<bool> m_allowFrameCapture{false};
    std::atomic<bool> m_controlDirty{false};

    QMutex m_switchLock;
    QString m_pendingActiveExe;
    quint32 m_pendingActivePid = 0;
    std::atomic<bool> m_switchRequested{false};

    QMutex m_textLock;
    QByteArray m_textSource;
    QByteArray m_textBody;
    bool m_textVisible = false;
    std::atomic<bool> m_textDirty{false};

    QMutex m_styleLock;
    QColor m_textColor{255, 255, 255, 255};
    QColor m_bgColor{0, 0, 0, 168};
    float m_fontSize = 0.0f;
    bool m_fontSizeAuto = false;
    int m_cornerRadius = 0;
    Qt::Alignment m_align = Qt::AlignHCenter;
    int m_marginTop = 0, m_marginBottom = 0, m_marginLeft = 0, m_marginRight = 0;
    float m_zoneX = 0.5f, m_zoneY = 0.88f, m_zoneW = 0.8f, m_zoneH = 0.5f;
    std::atomic<bool> m_styleDirty{false};

    QMutex m_fontLock;
    QByteArray m_fontPath;
    std::atomic<bool> m_fontDirty{false};
};

#endif // OVERLAYCAPTURESERVER_H
