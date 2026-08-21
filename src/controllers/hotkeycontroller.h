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

#ifndef HOTKEYCONTROLLER_H
#define HOTKEYCONTROLLER_H

#include <QObject>
#include <QKeySequence>

class HotKeys;
#ifdef Q_OS_LINUX
class PortalHotkeys;
#endif

class HotkeyController : public QObject
{
    Q_OBJECT

public:
    enum Mode {
        X11,
        Portal
    };

    explicit HotkeyController(QObject *parent = nullptr);
    void initialize(Mode mode);

    void setCaptureRegionShortcut(const QKeySequence &seq);
    void setShowHistoryShortcut(const QKeySequence &seq);
    void setRetranslateShortcut(const QKeySequence &seq);
    void setToggleSpeechShortcut(const QKeySequence &seq);
    void setSpeakTextShortcut(const QKeySequence &seq);
    void setStopSpeechShortcut(const QKeySequence &seq);

    // Portar backend
    void bindPortalShortcuts();

    Mode mode() const { return m_mode; }

signals:
    void captureRegionTriggered();
    void showHistoryTriggered();
    void retranslateTriggered();
    void toggleSpeechTriggered();
    void speakTextTriggered();
    void stopSpeechTriggered();

    // Portal-only event
    void shortcutReleased();

private slots:
    void onPortalActivated(const QString &shortcutId);
    void onPortalDeactivated();

private:
    Mode m_mode = X11;
    bool m_initialized = false;

    // Linux X11 and Windows
    HotKeys *m_captureRegionHotKey = nullptr;
    HotKeys *m_showHistoryHotKey = nullptr;
    HotKeys *m_retranslateHotKey = nullptr;
    HotKeys *m_toggleSpeechHotKey = nullptr;
    HotKeys *m_speakTextHotKey = nullptr;
    HotKeys *m_stopSpeechHotKey = nullptr;

#ifdef Q_OS_LINUX
    // Portal backend
    PortalHotkeys *m_portalHotKeys = nullptr;
#endif
};

#endif // HOTKEYCONTROLLER_H
