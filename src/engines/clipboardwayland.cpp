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

#include "clipboardwayland.h"

#ifdef AT_WAYLAND_DATA_CONTROL

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <wlr-data-control-unstable-v1-client-protocol.h>
#include <ext-data-control-v1-client-protocol.h>
#include <wayland-client-protocol.h>

#include <QGuiApplication>
#include <QSocketNotifier>
#include <QString>
#include <QByteArray>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// ===============================================================
// Listener structs
// ===============================================================

static const wl_registry_listener s_registryListener = {
    WaylandClipboardController::registryGlobal,
    WaylandClipboardController::registryGlobalRemove,
};

static const zwlr_data_control_device_v1_listener s_zwlrDeviceListener = {
    WaylandClipboardController::zwlrDataOffer,
    WaylandClipboardController::zwlrSelection,
    WaylandClipboardController::zwlrFinished,
    WaylandClipboardController::zwlrPrimarySelection,
};

static const zwlr_data_control_offer_v1_listener s_zwlrOfferListener = {
    WaylandClipboardController::zwlrOfferMime,
};

static const ext_data_control_device_v1_listener s_extDeviceListener = {
    WaylandClipboardController::extDataOffer,
    WaylandClipboardController::extSelection,
    WaylandClipboardController::extFinished,
    WaylandClipboardController::extPrimarySelection,
};

static const ext_data_control_offer_v1_listener s_extOfferListener = {
    WaylandClipboardController::extOfferMime,
};

// ===============================================================
// Constructor / Destructor
// ===============================================================

WaylandClipboardController::WaylandClipboardController(QObject *parent)
    : ClipboardController(parent)
{
    connectToDisplay();
}

WaylandClipboardController::~WaylandClipboardController()
{
    stop();

    if (m_zwlrDevice)  { zwlr_data_control_device_v1_destroy(m_zwlrDevice);   m_zwlrDevice  = nullptr; }
    if (m_zwlrManager) { zwlr_data_control_manager_v1_destroy(m_zwlrManager); m_zwlrManager = nullptr; }
    if (m_extDevice)   { ext_data_control_device_v1_destroy(m_extDevice);     m_extDevice   = nullptr; }
    if (m_extManager)  { ext_data_control_manager_v1_destroy(m_extManager);   m_extManager  = nullptr; }
    if (m_seat)        { wl_seat_destroy(m_seat);                             m_seat        = nullptr; }
    if (m_registry)    { wl_registry_destroy(m_registry);                     m_registry    = nullptr; }

    if (m_display && m_ownDisplay) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

// ===============================================================
// Public API
// ===============================================================

bool WaylandClipboardController::isSupported() const
{
    return m_supported;
}

void WaylandClipboardController::start()
{
    if (!m_supported || !m_display || !m_seat || m_notifier)
        return;

    if (m_extManager && !m_extDevice) {
        m_extDevice = ext_data_control_manager_v1_get_data_device(m_extManager, m_seat);
        ext_data_control_device_v1_add_listener(m_extDevice, &s_extDeviceListener, this);
        wl_display_flush(m_display);
    } else if (m_zwlrManager && !m_zwlrDevice) {
        m_zwlrDevice = zwlr_data_control_manager_v1_get_data_device(m_zwlrManager, m_seat);
        zwlr_data_control_device_v1_add_listener(m_zwlrDevice, &s_zwlrDeviceListener, this);
        wl_display_flush(m_display);
    }

    int fd = wl_display_get_fd(m_display);
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &WaylandClipboardController::onDisplayReadable);
}

void WaylandClipboardController::stop()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

    if (m_zwlrPendingOffer) {
        zwlr_data_control_offer_v1_destroy(m_zwlrPendingOffer);
        m_zwlrPendingOffer = nullptr;
    }

    if (m_extPendingOffer) {
        ext_data_control_offer_v1_destroy(m_extPendingOffer);
        m_extPendingOffer = nullptr;
    }
}

// ===============================================================
// Private helpers
// ===============================================================

void WaylandClipboardController::connectToDisplay()
{
    m_display = wl_display_connect(nullptr);
    if (!m_display)
        return;

    m_ownDisplay = true;

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &s_registryListener, this);

    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);
}

void WaylandClipboardController::readPipe(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    auto *buf      = new QByteArray();
    auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);

    connect(notifier, &QSocketNotifier::activated, this, [this, notifier, buf, fd]() {
        char tmp[4096];
        ssize_t n;
        while ((n = read(fd, tmp, sizeof(tmp))) > 0)
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

void WaylandClipboardController::readZwlrOffer(zwlr_data_control_offer_v1 *offer)
{
    if (!offer) return;

    static const char * const kMimes[] = {
        "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", nullptr
    };

    QString mime;
    for (int i = 0; kMimes[i]; ++i) {
        if (m_pendingMimes.contains(QString::fromLatin1(kMimes[i]))) {
            mime = QString::fromLatin1(kMimes[i]);
            break;
        }
    }
    if (mime.isEmpty()) return;

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) == -1) return;

    zwlr_data_control_offer_v1_receive(offer, mime.toLatin1().constData(), fds[1]);
    zwlr_data_control_offer_v1_destroy(offer);
    m_zwlrSelectionOffer = nullptr;
    wl_display_flush(m_display);
    close(fds[1]);
    readPipe(fds[0]);
}

void WaylandClipboardController::readExtOffer(ext_data_control_offer_v1 *offer)
{
    if (!offer) return;

    static const char * const kMimes[] = {
        "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", nullptr
    };

    QString mime;
    for (int i = 0; kMimes[i]; ++i) {
        if (m_pendingMimes.contains(QString::fromLatin1(kMimes[i]))) {
            mime = QString::fromLatin1(kMimes[i]);
            break;
        }
    }
    if (mime.isEmpty()) return;

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) == -1) return;

    ext_data_control_offer_v1_receive(offer, mime.toLatin1().constData(), fds[1]);
    ext_data_control_offer_v1_destroy(offer);
    m_extSelectionOffer = nullptr;
    wl_display_flush(m_display);
    close(fds[1]);
    readPipe(fds[0]);
}

void WaylandClipboardController::onDisplayReadable()
{
    if (wl_display_prepare_read(m_display) == 0)
        wl_display_read_events(m_display);

    wl_display_dispatch_pending(m_display);
    wl_display_flush(m_display);
}

// ===============================================================
// wl_registry callbacks
// ===============================================================

void WaylandClipboardController::registryGlobal(void *data,
                                                wl_registry *registry,
                                                uint32_t name,
                                                const char *interface,
                                                uint32_t version)
{
    auto *self = static_cast<WaylandClipboardController*>(data);

    if (strcmp(interface, ext_data_control_manager_v1_interface.name) == 0) {
        self->m_extManager = static_cast<ext_data_control_manager_v1 *>(
            wl_registry_bind(registry, name,
                             &ext_data_control_manager_v1_interface,
                             qMin(version, 1u)));
        if (self->m_extManager)
            self->m_supported = true;
    } else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0) {
        if (!self->m_extManager) {
            self->m_zwlrManager = static_cast<zwlr_data_control_manager_v1 *>(
                wl_registry_bind(registry, name,
                                 &zwlr_data_control_manager_v1_interface,
                                 qMin(version, 2u)));
            if (self->m_zwlrManager)
                self->m_supported = true;
        }
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !self->m_seat) {
        self->m_seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, 1));
    }
}

void WaylandClipboardController::registryGlobalRemove(void *, wl_registry *, uint32_t) {}

// ===============================================================
// zwlr callbacks
// ===============================================================

void WaylandClipboardController::zwlrDataOffer(void *data, zwlr_data_control_device_v1 *, zwlr_data_control_offer_v1 *offer)
{
    auto *self = static_cast<WaylandClipboardController*>(data);
    if (self->m_zwlrPendingOffer && self->m_zwlrPendingOffer != self->m_zwlrSelectionOffer)
        zwlr_data_control_offer_v1_destroy(self->m_zwlrPendingOffer);
    self->m_zwlrPendingOffer = offer;
    self->m_pendingMimes.clear();
    zwlr_data_control_offer_v1_add_listener(offer, &s_zwlrOfferListener, self);
}

void WaylandClipboardController::zwlrSelection(void *data, zwlr_data_control_device_v1 *, zwlr_data_control_offer_v1 *offer)
{
    auto *self = static_cast<WaylandClipboardController*>(data);
    if (self->m_zwlrSelectionOffer) {
        zwlr_data_control_offer_v1_destroy(self->m_zwlrSelectionOffer);
        self->m_zwlrSelectionOffer = nullptr;
    }
    self->m_zwlrPendingOffer = nullptr;
    if (!offer) { self->m_pendingMimes.clear(); return; }

    self->m_zwlrSelectionOffer = offer;
    self->readZwlrOffer(offer);
    self->m_pendingMimes.clear();
}

void WaylandClipboardController::zwlrFinished(void *data, zwlr_data_control_device_v1 *)
{
    static_cast<WaylandClipboardController*>(data)->stop();
}

void WaylandClipboardController::zwlrPrimarySelection(void *data, zwlr_data_control_device_v1 *, zwlr_data_control_offer_v1 *offer)
{
    auto *self = static_cast<WaylandClipboardController*>(data);
    if (offer && offer == self->m_zwlrPendingOffer) {
        zwlr_data_control_offer_v1_destroy(offer);
        self->m_zwlrPendingOffer = nullptr;
    } else if (offer) {
        zwlr_data_control_offer_v1_destroy(offer);
    }
    self->m_pendingMimes.clear();
}

void WaylandClipboardController::zwlrOfferMime(void *data, zwlr_data_control_offer_v1 *, const char *mime_type)
{
    static_cast<WaylandClipboardController*>(data)->m_pendingMimes.append(QString::fromLatin1(mime_type));
}

// ===============================================================
// ext callbacks
// ===============================================================

void WaylandClipboardController::extDataOffer(void *data, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer)
{
    auto *self = static_cast<WaylandClipboardController*>(data);
    if (self->m_extPendingOffer && self->m_extPendingOffer != self->m_extSelectionOffer)
        ext_data_control_offer_v1_destroy(self->m_extPendingOffer);
    self->m_extPendingOffer = offer;
    self->m_pendingMimes.clear();
    ext_data_control_offer_v1_add_listener(offer, &s_extOfferListener, self);
}

void WaylandClipboardController::extSelection(void *data, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer)
{
    auto *self = static_cast<WaylandClipboardController*>(data);
    if (self->m_extSelectionOffer) {
        ext_data_control_offer_v1_destroy(self->m_extSelectionOffer);
        self->m_extSelectionOffer = nullptr;
    }
    self->m_extPendingOffer = nullptr;
    if (!offer) { self->m_pendingMimes.clear(); return; }

    self->m_extSelectionOffer = offer;
    self->readExtOffer(offer);
    self->m_pendingMimes.clear();
}

void WaylandClipboardController::extFinished(void *data, ext_data_control_device_v1 *)
{
    static_cast<WaylandClipboardController*>(data)->stop();
}

void WaylandClipboardController::extPrimarySelection(void *data, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer)
{
    auto *self = static_cast<WaylandClipboardController*>(data);
    if (offer && offer == self->m_extPendingOffer) {
        ext_data_control_offer_v1_destroy(offer);
        self->m_extPendingOffer = nullptr;
    } else if (offer) {
        ext_data_control_offer_v1_destroy(offer);
    }
    self->m_pendingMimes.clear();
}

void WaylandClipboardController::extOfferMime(void *data, ext_data_control_offer_v1 *, const char *mime_type)
{
    static_cast<WaylandClipboardController*>(data)->m_pendingMimes.append(QString::fromLatin1(mime_type));
}

#endif // AT_WAYLAND_DATA_CONTROL
