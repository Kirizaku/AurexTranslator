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

#ifndef CLIPBOARDWAYLAND_H
#define CLIPBOARDWAYLAND_H

#ifdef AT_WAYLAND_DATA_CONTROL

#include "../controllers/clipboardcontroller.h"

#include <QSocketNotifier>
#include <QStringList>

struct wl_display;
struct wl_registry;
struct wl_seat;

// zwlr (Hyprland / Sway)
struct zwlr_data_control_manager_v1;
struct zwlr_data_control_device_v1;
struct zwlr_data_control_offer_v1;

// ext (KDE Plasma / Hyprland / Sway)
struct ext_data_control_manager_v1;
struct ext_data_control_device_v1;
struct ext_data_control_offer_v1;

class WaylandClipboardController : public ClipboardController
{
    Q_OBJECT

public:
    explicit WaylandClipboardController(QObject *parent = nullptr);
    ~WaylandClipboardController() override;

    void start() override;
    void stop() override;

    bool isSupported() const;

    // wl_registry
    static void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t name);

    // zwlr device callbacks
    static void zwlrDataOffer(void *data, zwlr_data_control_device_v1 *dev, zwlr_data_control_offer_v1 *offer);
    static void zwlrSelection(void *data, zwlr_data_control_device_v1 *dev, zwlr_data_control_offer_v1 *offer);
    static void zwlrFinished(void *data, zwlr_data_control_device_v1 *dev);
    static void zwlrPrimarySelection(void *data, zwlr_data_control_device_v1 *dev, zwlr_data_control_offer_v1 *offer);
    static void zwlrOfferMime(void *data, zwlr_data_control_offer_v1 *offer, const char *mime_type);

    // ext device callbacks
    static void extDataOffer(void *data, ext_data_control_device_v1 *dev, ext_data_control_offer_v1 *offer);
    static void extSelection(void *data, ext_data_control_device_v1 *dev, ext_data_control_offer_v1 *offer);
    static void extFinished(void *data, ext_data_control_device_v1 *dev);
    static void extPrimarySelection(void *data, ext_data_control_device_v1 *dev, ext_data_control_offer_v1 *offer);
    static void extOfferMime(void *data, ext_data_control_offer_v1 *offer, const char *mime_type);

private:
    wl_display   *m_display  = nullptr;
    wl_registry  *m_registry = nullptr;
    wl_seat      *m_seat     = nullptr;
    bool          m_ownDisplay = false;

    // zwlr path
    zwlr_data_control_manager_v1 *m_zwlrManager = nullptr;
    zwlr_data_control_device_v1  *m_zwlrDevice  = nullptr;
    zwlr_data_control_offer_v1   *m_zwlrPendingOffer   = nullptr;
    zwlr_data_control_offer_v1   *m_zwlrSelectionOffer = nullptr;

    // ext path
    ext_data_control_manager_v1  *m_extManager = nullptr;
    ext_data_control_device_v1   *m_extDevice  = nullptr;
    ext_data_control_offer_v1    *m_extPendingOffer   = nullptr;
    ext_data_control_offer_v1    *m_extSelectionOffer = nullptr;

    // shared pending MIME list
    QStringList m_pendingMimes;

    bool m_supported = false;

    QSocketNotifier *m_notifier = nullptr;

    void connectToDisplay();

    // receive() and read()
    void readZwlrOffer(zwlr_data_control_offer_v1 *offer);
    void readExtOffer(ext_data_control_offer_v1 *offer);
    void readPipe(int fd);

private slots:
    void onDisplayReadable();
};

#endif // AT_WAYLAND_DATA_CONTROL
#endif // CLIPBOARDWAYLAND_H
