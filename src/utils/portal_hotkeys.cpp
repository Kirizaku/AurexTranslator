/******************************************************************************
    Copyright (C) 2025 by Daniil Nabiulin

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

#include "portal_hotkeys.h"
#include "src/utils/logger.h"

using Shortcuts = QList<QPair<QString, QVariantMap>>;

PortalHotkeys::PortalHotkeys(QObject *parent)
    : QObject(parent)
{}

PortalHotkeys::~PortalHotkeys()
{
    delete m_portalShortcuts;
}

bool PortalHotkeys::init()
{
    qDBusRegisterMetaType<Shortcuts>();
    qDBusRegisterMetaType<QPair<QString,QVariantMap>>();

    m_portalShortcuts = new OrgFreedesktopPortalGlobalShortcutsInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                                         QLatin1String("/org/freedesktop/portal/desktop"),
                                                                         QDBusConnection::sessionBus(), this);

    auto reply = m_portalShortcuts->CreateSession({
                                                   { QLatin1String("session_handle_token"), QString("g%1").arg(m_sessionTokenCounter += 1) },
                                                   { QLatin1String("handle_token"), QString("g%1").arg(m_requestTokenCounter += 1) },
                                                   });

    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[shortcuts] Couldn't get reply: %1").arg(reply.error().message()));
    } else {
        QDBusConnection::sessionBus().connect(QString(),
                                              reply.value().path(),
                                              QLatin1String("org.freedesktop.portal.Request"),
                                              QLatin1String("Response"),
                                              this,
                                              SLOT(gotGlobalShortcutsCreateSessionResponse(uint,QVariantMap)));
    }

    connect(m_portalShortcuts, &OrgFreedesktopPortalGlobalShortcutsInterface::Activated,
            this, &PortalHotkeys::handleActivated);
    connect(m_portalShortcuts, &OrgFreedesktopPortalGlobalShortcutsInterface::Deactivated,
            this, &PortalHotkeys::handleDeactivated);

    return true;
}

void PortalHotkeys::gotGlobalShortcutsCreateSessionResponse(uint response, const QVariantMap& results)
{
    if (response != 0) {
        Log(Logger::Level::Warning, QString("[shortcuts] Failed to create a global shortcuts session: %1").arg(response));
        return;
    }

    m_globalShortcutsSession = QDBusObjectPath(results["session_handle"].toString());
}

void PortalHotkeys::bindShortcuts()
{
    Shortcuts shortcuts = {
        { QStringLiteral("CaptureRegion"), { { QStringLiteral("description"), QStringLiteral("Capture OCR Region") } } },
        { QStringLiteral("HistoryTranslation"), { { QStringLiteral("description"), QStringLiteral("Show/Hide History Translation") } } }
    };

    auto reply = m_portalShortcuts->BindShortcuts(m_globalShortcutsSession, shortcuts, 0, {{ "handle_token", QString("g%1").arg(m_requestTokenCounter += 1) }} );
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[shortcuts] Failed to call BindShortcuts: %1").arg(reply.error().message()));
        return;
    }
}

void PortalHotkeys::handleActivated(const QDBusObjectPath &session_handle, const QString &shortcut_id, qulonglong timestamp, const QVariantMap &options)
{
    emit activated(shortcut_id);
}

void PortalHotkeys::handleDeactivated()
{
    emit deactivated();
}
