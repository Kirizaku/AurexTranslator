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

#ifndef HOOKSELECTORDIALOG_H
#define HOOKSELECTORDIALOG_H

#include <QDialog>
#include <QSet>

class HookTextModel;
class QComboBox;
class QLabel;
class QLayout;
class QVBoxLayout;
class QPushButton;
class QSpinBox;
class QWidget;
class QTimer;

class HookSelectorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HookSelectorDialog(HookTextModel *model, QWidget *parent = nullptr);
    void openFresh();

private:
    void refresh();
    void refreshScopedSettings();
    void updateGroupButton();

    HookTextModel *m_model;
    QVBoxLayout *m_liveList = nullptr;
    QVBoxLayout *m_savedList = nullptr;
    QWidget *m_waitWidget = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QSpinBox *m_waitSpin = nullptr;
    QPushButton *m_groupBtn = nullptr;
    QTimer *m_refreshTimer = nullptr;
    bool m_addrSaveWarned = false;
    QSet<QString> m_groupPick;
    QString m_filter;
};

#endif // HOOKSELECTORDIALOG_H
