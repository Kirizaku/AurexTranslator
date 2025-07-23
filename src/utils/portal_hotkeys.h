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

#ifndef PORTAL_HOTKEYS_H
#define PORTAL_HOTKEYS_H

#include <QObject>
#include <globalshortcuts_portal_interface.h>

class PortalHotkeys : public QObject
{
    Q_OBJECT
public:
    explicit PortalHotkeys(QObject *parent = nullptr);
    ~PortalHotkeys();

    bool init();
    void bindShortcuts();

signals:
    void activated(const QString &shortcutId);
    void deactivated();

private slots:
    void gotGlobalShortcutsCreateSessionResponse(uint res, const QVariantMap& results);
    void handleActivated(const QDBusObjectPath &session_handle, const QString &shortcut_id, qulonglong timestamp, const QVariantMap &options);
    void handleDeactivated();

private:
    OrgFreedesktopPortalGlobalShortcutsInterface *m_portalShortcuts = nullptr;
    QDBusObjectPath m_globalShortcutsSession;
    int m_sessionTokenCounter = 0;
    int m_requestTokenCounter = 0;
};

#endif // PORTAL_HOTKEYS_H
