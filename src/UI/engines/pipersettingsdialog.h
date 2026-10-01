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

#ifndef PIPERSETTINGSDIALOG_H
#define PIPERSETTINGSDIALOG_H

#include <QDialog>

class PiperTts;
class PiperVoiceCatalog;

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;

class PiperSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    PiperSettingsDialog(PiperTts *piper, QWidget *parent = nullptr);

private:
    void refreshState();
    void refreshVoices();
    void downloadSelected();

    PiperTts *m_piper = nullptr;
    PiperVoiceCatalog *m_catalog = nullptr;

    QRadioButton *m_modeManaged = nullptr;
    QRadioButton *m_modeExternal = nullptr;
    QLineEdit *m_serverUrl = nullptr;
    QPushButton *m_checkButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QGroupBox *m_voiceBox = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QComboBox *m_voiceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_downloadButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progress = nullptr;
    bool m_cancelRequested = false;

    QLineEdit *m_repoEdit = nullptr;
    QPushButton *m_repoResetButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
};

#endif // PIPERSETTINGSDIALOG_H
