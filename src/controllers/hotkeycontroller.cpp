/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

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

#include "hotkeycontroller.h"

#include "src/utils/hotkeys.h"
#ifdef Q_OS_LINUX
#include "src/utils/portal_hotkeys.h"
#endif

HotkeyController::HotkeyController(QObject *parent)
    : QObject(parent) {}

void HotkeyController::initialize(Mode mode)
{
    if (m_initialized) return;

    m_initialized = true;
    m_mode = mode;

    if (m_mode == X11) {
        m_captureRegionHotKey = new HotKeys(this);
        connect(m_captureRegionHotKey, &HotKeys::activated,
                this, &HotkeyController::captureRegionTriggered);

        m_showHistoryHotKey = new HotKeys(this);
        connect(m_showHistoryHotKey, &HotKeys::activated,
                this, &HotkeyController::showHistoryTriggered);

        m_retranslateHotKey = new HotKeys(this);
        connect(m_retranslateHotKey, &HotKeys::activated,
                this, &HotkeyController::retranslateTriggered);

        m_toggleSpeechHotKey = new HotKeys(this);
        connect(m_toggleSpeechHotKey, &HotKeys::activated,
                this, &HotkeyController::toggleSpeechTriggered);

        m_speakTextHotKey = new HotKeys(this);
        connect(m_speakTextHotKey, &HotKeys::activated,
                this, &HotkeyController::speakTextTriggered);

        m_stopSpeechHotKey = new HotKeys(this);
        connect(m_stopSpeechHotKey, &HotKeys::activated,
                this, &HotkeyController::stopSpeechTriggered);
    }
#ifdef Q_OS_LINUX
    else {
        m_portalHotKeys = new PortalHotkeys(this);
        m_portalHotKeys->init();
        connect(m_portalHotKeys, &PortalHotkeys::activated,
                this, &HotkeyController::onPortalActivated);
        connect(m_portalHotKeys, &PortalHotkeys::deactivated,
                this, &HotkeyController::onPortalDeactivated);
    }
#endif
}

void HotkeyController::setCaptureRegionShortcut(const QKeySequence &seq)
{
    if (m_captureRegionHotKey) m_captureRegionHotKey->setShortcut(seq);
}

void HotkeyController::setShowHistoryShortcut(const QKeySequence &seq)
{
    if (m_showHistoryHotKey) m_showHistoryHotKey->setShortcut(seq);
}

void HotkeyController::setRetranslateShortcut(const QKeySequence &seq)
{
    if (m_retranslateHotKey) m_retranslateHotKey->setShortcut(seq);
}

void HotkeyController::setToggleSpeechShortcut(const QKeySequence &seq)
{
    if (m_toggleSpeechHotKey) m_toggleSpeechHotKey->setShortcut(seq);
}

void HotkeyController::setSpeakTextShortcut(const QKeySequence &seq)
{
    if (m_speakTextHotKey) m_speakTextHotKey->setShortcut(seq);
}

void HotkeyController::setStopSpeechShortcut(const QKeySequence &seq)
{
    if (m_stopSpeechHotKey) m_stopSpeechHotKey->setShortcut(seq);
}

void HotkeyController::bindPortalShortcuts()
{
#ifdef Q_OS_LINUX
    if (m_portalHotKeys) m_portalHotKeys->bindShortcuts();
#endif
}

void HotkeyController::onPortalActivated(const QString &shortcutId)
{
    if (shortcutId == QStringLiteral("CaptureRegion")) {
        emit captureRegionTriggered();
    } else if (shortcutId == QStringLiteral("HistoryTranslation")) {
        emit showHistoryTriggered();
    } else if (shortcutId == QStringLiteral("ManualTranslate")) {
        emit retranslateTriggered();
    } else if (shortcutId == QStringLiteral("ToggleSpeech")) {
        emit toggleSpeechTriggered();
    } else if (shortcutId == QStringLiteral("SpeakText")) {
        emit speakTextTriggered();
    } else if (shortcutId == QStringLiteral("StopSpeech")) {
        emit stopSpeechTriggered();
    }
}

void HotkeyController::onPortalDeactivated()
{
    emit shortcutReleased();
}
