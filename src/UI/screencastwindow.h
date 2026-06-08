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

#ifndef SCREENCASTWINDOW_H
#define SCREENCASTWINDOW_H

#include <QWidget>
#include <QAbstractButton>

#ifdef __linux__
#include "src/screencasts/linux-capture-x11/screencast-x11.h"

// Xlib defines several uppercase macros (Always, None, Bool, Status...) that collide with Qt

#ifdef Always
#  undef Always
#endif
#ifdef None
#  undef None
#endif
#ifdef Bool
#  undef Bool
#endif
#ifdef Status
#  undef Status
#endif
#ifdef CursorShape
#  undef CursorShape
#endif
#ifdef KeyPress
#  undef KeyPress
#endif
#ifdef KeyRelease
#  undef KeyRelease
#endif
#ifdef FocusIn
#  undef FocusIn
#endif
#ifdef FocusOut
#  undef FocusOut
#endif
#ifdef FontChange
#  undef FontChange
#endif
#ifdef Expose
#  undef Expose
#endif
#ifdef Unsorted
#  undef Unsorted
#endif
#else
#include "src/screencasts/win-capture/screencast-win.h"
#endif

namespace Ui {
class ScreenCastWindow;
}

class ScreenCastWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenCastWindow(ScreenCast* screenCapture, QWidget *parent = nullptr);
    ~ScreenCastWindow();

public slots:
    void setCurrentOriginalFrame(const QImage &frame);

private slots:
    void on_desktopRadio_toggled(bool checked);
    void on_listComboBox_currentIndexChanged(int index);
    void on_pushButton_clicked();
    void on_buttonBox_clicked(QAbstractButton *button);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

signals:
    void screencastWindowShown();
    void screencastWindowHidden();

private:
    Ui::ScreenCastWindow *ui;
    ScreenCast *m_screenCapture;

    QVector<DisplayInfo> m_currentDisplaysList;
    QVector<WindowInfo> m_currentWindowsList;

    void onWidgetChanged();
    void setCurrentApplyButtonState(bool value);
    void updateList();
    void updateDisplayList();
    void updateWindowList();
    void loadConfig();
    void saveConfig();
};

#endif // SCREENCASTWINDOW_H