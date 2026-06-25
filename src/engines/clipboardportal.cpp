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

#include "clipboardportal.h"
#include "src/utils/logger.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusUnixFileDescriptor>
#include <QSocketNotifier>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

PortalClipboardController::PortalClipboardController(QObject *parent)
    : ClipboardController(parent)
{ }

PortalClipboardController::~PortalClipboardController()
{
    stop();
}

bool PortalClipboardController::isSupported() const
{
    if (!QDBusConnection::sessionBus().isConnected())
        return false;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String("org.freedesktop.portal.Desktop"),
        QLatin1String("/org/freedesktop/portal/desktop"),
        QLatin1String("org.freedesktop.DBus.Properties"),
        QLatin1String("Get"));
    msg << QLatin1String("org.freedesktop.portal.Clipboard") << QLatin1String("version");

    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return false;

    const uint version = reply.arguments().first().value<QDBusVariant>().variant().toUInt();
    return version >= 1;
}

QString PortalClipboardController::getSessionToken()
{
    m_sessionTokenCounter += 1;
    return QString("cb%1").arg(m_sessionTokenCounter);
}

QString PortalClipboardController::getRequestToken()
{
    m_requestTokenCounter += 1;
    return QString("cb%1").arg(m_requestTokenCounter);
}

void PortalClipboardController::start()
{
    if (m_remoteDesktop)
        return;

    m_remoteDesktop = new OrgFreedesktopPortalRemoteDesktopInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                                     QLatin1String("/org/freedesktop/portal/desktop"),
                                                                     QDBusConnection::sessionBus(), this);

    m_clipboard = new OrgFreedesktopPortalClipboardInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                             QLatin1String("/org/freedesktop/portal/desktop"),
                                                             QDBusConnection::sessionBus(), this);

    auto reply = m_remoteDesktop->CreateSession({
                                                 { QLatin1String("session_handle_token"), getSessionToken() },
                                                 { QLatin1String("handle_token"), getRequestToken() },
                                                 });
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[portal-clip] Couldn't get reply: %1").arg(reply.error().message()));
    } else {
        QDBusConnection::sessionBus().connect(QString(),
                                              reply.value().path(),
                                              QLatin1String("org.freedesktop.portal.Request"),
                                              QLatin1String("Response"),
                                              this,
                                              SLOT(gotCreateSessionResponse(uint,QVariantMap)));
    }
}

void PortalClipboardController::stop()
{
    if (!m_remoteDesktop)
        return;

    QDBusConnection::sessionBus().disconnect(QLatin1String("org.freedesktop.portal.Desktop"),
                                             QString(),
                                             QLatin1String("org.freedesktop.portal.Clipboard"),
                                             QLatin1String("SelectionOwnerChanged"),
                                             this,
                                             SLOT(onSelectionOwnerChanged(QDBusObjectPath,QVariantMap)));

    if (!m_session.path().isEmpty()) {
        QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                              m_session.path(),
                                                              QLatin1String("org.freedesktop.portal.Session"),
                                                              QLatin1String("Close"));
        QDBusConnection::sessionBus().asyncCall(message);
        m_session = QDBusObjectPath();
    }

    delete m_clipboard;
    m_clipboard = nullptr;

    delete m_remoteDesktop;
    m_remoteDesktop = nullptr;
}

void PortalClipboardController::gotCreateSessionResponse(uint response, const QVariantMap &results)
{
    if (!m_remoteDesktop) return;

    if (response != 0) {
        Log(Logger::Level::Warning, QString("[portal-clip] Failed to create session: %1").arg(response));
        return;
    }

    Log(Logger::Level::Info, "[portal-clip] Clipboard session created");

    m_session = QDBusObjectPath(results["session_handle"].toString());

    auto reply = m_remoteDesktop->SelectDevices(m_session, {
        { QLatin1String("handle_token"), getRequestToken() },
        { QLatin1String("types"),        uint(1) },
    });
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[portal-clip] Failed to call SelectDevices: %1").arg(reply.error().message()));
        return;
    }

    QDBusConnection::sessionBus().connect(QString(),
                                          reply.value().path(),
                                          QLatin1String("org.freedesktop.portal.Request"),
                                          QLatin1String("Response"),
                                          this,
                                          SLOT(gotSelectDevicesResponse(uint,QVariantMap)));
}

void PortalClipboardController::gotSelectDevicesResponse(uint response, const QVariantMap &results)
{
    if (!m_remoteDesktop) return;

    if (response != 0) {
        Log(Logger::Level::Warning, QString("[portal-clip] Failed to select devices: %1").arg(response));
        return;
    }

    // RequestClipboard must be called before Start
    auto clipReply = m_clipboard->RequestClipboard(m_session, {});
    clipReply.waitForFinished();
    if (clipReply.isError()) {
        Log(Logger::Level::Warning, QString("[portal-clip] Failed to call RequestClipboard: %1").arg(clipReply.error().message()));
    }

    auto reply = m_remoteDesktop->Start(m_session, QString(), {{ QLatin1String("handle_token"), getRequestToken() }});
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[portal-clip] Failed to call Start: %1").arg(reply.error().message()));
        return;
    }

    QDBusConnection::sessionBus().connect(QString(),
                                          reply.value().path(),
                                          QLatin1String("org.freedesktop.portal.Request"),
                                          QLatin1String("Response"),
                                          this,
                                          SLOT(gotStartResponse(uint,QVariantMap)));
}

void PortalClipboardController::gotStartResponse(uint response, const QVariantMap &results)
{
    if (!m_remoteDesktop) return;

    if (response != 0) {
        Log(Logger::Level::Warning, QString("[portal-clip] Start denied or cancelled: %1").arg(response));
        emit failed();
        return;
    }

    const QVariant clipboardEnabled = results.value(QLatin1String("clipboard_enabled"));
    if (clipboardEnabled.isValid() && !clipboardEnabled.toBool()) {
        Log(Logger::Level::Warning, "[portal-clip] Clipboard access not granted");
        emit failed();
        return;
    }

    QDBusConnection::sessionBus().connect(QLatin1String("org.freedesktop.portal.Desktop"),
                                          QString(),
                                          QLatin1String("org.freedesktop.portal.Clipboard"),
                                          QLatin1String("SelectionOwnerChanged"),
                                          this,
                                          SLOT(onSelectionOwnerChanged(QDBusObjectPath,QVariantMap)));
}

void PortalClipboardController::onSelectionOwnerChanged(const QDBusObjectPath &session_handle,
                                                         const QVariantMap &options)
{
    if (session_handle.path() != m_session.path())
        return;
    if (options.value(QLatin1String("session_is_owner")).toBool())
        return;

    const QString mime = pickMime(options.value(QLatin1String("mime_types")).toStringList());
    if (!mime.isEmpty())
        readSelection(mime);
}

void PortalClipboardController::readSelection(const QString &mime)
{
    auto reply = m_clipboard->SelectionRead(m_session, mime);
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[portal-clip] SelectionRead failed: %1").arg(reply.error().message()));
        return;
    }

    const QDBusUnixFileDescriptor qfd = reply.value();
    if (!qfd.isValid())
        return;

    int fd = ::dup(qfd.fileDescriptor());

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    auto *buf      = new QByteArray;
    auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);

    connect(notifier, &QSocketNotifier::activated, this, [this, notifier, buf, fd]() {
        char tmp[4096];
        ssize_t n;
        while ((n = read(fd, tmp, sizeof tmp)) > 0)
            buf->append(tmp, static_cast<int>(n));

        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            notifier->setEnabled(false);
            notifier->deleteLater();
            close(fd);

            const QString text = QString::fromUtf8(*buf).trimmed();
            delete buf;

            if (!text.isEmpty() && !checkAndClearSuppressed(text))
                emit textChanged(text);
        }
    });
}

QString PortalClipboardController::pickMime(const QStringList &mimes) const
{
    for (const auto *want : {"text/plain;charset=utf-8", "text/plain",
                             "UTF8_STRING", "STRING"})
        if (mimes.contains(QLatin1String(want)))
            return QString::fromLatin1(want);
    return {};
}
