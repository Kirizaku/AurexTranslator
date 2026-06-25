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

#ifndef CLIPBOARDPORTAL_H
#define CLIPBOARDPORTAL_H

#include "../controllers/clipboardcontroller.h"

#include <QDBusObjectPath>
#include <remotedesktop_portal_interface.h>
#include <clipboard_portal_interface.h>

class OrgFreedesktopPortalRemoteDesktopInterface;
class OrgFreedesktopPortalClipboardInterface;

class PortalClipboardController : public ClipboardController
{
    Q_OBJECT
public:
    explicit PortalClipboardController(QObject *parent = nullptr);
    ~PortalClipboardController();

    void start() override;
    void stop()  override;

    bool isSupported() const;

public Q_SLOTS:
    void gotCreateSessionResponse(uint response, const QVariantMap &results);
    void gotSelectDevicesResponse(uint response, const QVariantMap &results);
    void gotStartResponse(uint response, const QVariantMap &results);
    void onSelectionOwnerChanged(const QDBusObjectPath &session_handle, const QVariantMap &options);

private:
    OrgFreedesktopPortalRemoteDesktopInterface *m_remoteDesktop = nullptr;
    OrgFreedesktopPortalClipboardInterface     *m_clipboard     = nullptr;
    QDBusObjectPath m_session;
    uint m_sessionTokenCounter = 0;
    uint m_requestTokenCounter = 0;

    QString getSessionToken();
    QString getRequestToken();
    void readSelection(const QString &mime);
    QString pickMime(const QStringList &mimes) const;
};

#endif // CLIPBOARDPORTAL_H
