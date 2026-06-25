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

#include "clipboardcontroller.h"
#include "../engines/clipboardqt.h"

#ifdef Q_OS_LINUX
#   ifdef AT_WAYLAND_DATA_CONTROL
#       include "../engines/clipboardwayland.h"
#   endif
#       include "../engines/clipboardportal.h"
#endif

#include <QGuiApplication>

ClipboardController *ClipboardController::create(QObject *parent)
{
#ifdef Q_OS_LINUX
    if (qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland") {
        // Native Wayland data-control (KDE Plasma / Hyprland / Sway)
#  ifdef AT_WAYLAND_DATA_CONTROL
        auto *wl = new WaylandClipboardController(parent);
        if (wl->isSupported())
            return wl;
        delete wl;
#  endif
        // XDG portal (GNOME Wayland)
        auto *portal = new PortalClipboardController(parent);
        if (portal->isSupported())
            return portal;
        delete portal;
    }
#endif

    // Linux X11, Windows
    return new QtClipboardController(parent);
}
