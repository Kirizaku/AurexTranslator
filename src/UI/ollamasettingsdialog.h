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

#ifndef OLLAMASETTINGSDIALOG_H
#define OLLAMASETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QJsonArray>
#include <QRadioButton>
#include <QSpinBox>
#include <QCheckBox>

#include "../engines/ollama.h"

class OllamaSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OllamaSettingsDialog(Ollama *ollama, QString &currentModel, QJsonArray &models, QWidget *parent = nullptr);
    ~OllamaSettingsDialog();

    QString getUrl() const;
    QString getCurrentModel() const;
    QJsonArray getModels() const;
    QString getTranslationPrompt() const;
    QString getVisionPrompt() const;
    int getMode() const;
    int getAutoInterval() const;
    bool getIsWaitForResponse() const;
    void setCurrentSettings(const QString &url, QString &currentModel, const QString &prompt, const QString &visionPrompt, const int &visionMode, const int &visionAutoInterval, const bool &waitForOllamaResponse);

private slots:
    void updateList();

private:
    Ollama *m_ollama;
    QLabel *statusLabel;
    QPushButton *updateButton;
    QLineEdit *m_lineEdit;
    QComboBox *m_comboBox;
    QPlainTextEdit *m_translationPromptEdit;
    QPlainTextEdit *m_visionPromptEdit;
    QRadioButton *m_autoRadio;
    QRadioButton *m_manualRadio;
    QButtonGroup *m_modeGroup;
    QSpinBox *m_intervalSpinBox;
    QCheckBox *m_ollamaWaitResponseCheckBox;

    enum Mode {
        Auto,
        Manual
    };
};

#endif // OLLAMASETTINGSDIALOG_H
