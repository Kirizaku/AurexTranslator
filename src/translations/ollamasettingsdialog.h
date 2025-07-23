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

#include "ollama.h"

class OllamaSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OllamaSettingsDialog(const QString &url, const QString &currentModel, const QStringList &models, const QString &prompt, Ollama *ollama, QWidget *parent = nullptr);
    ~OllamaSettingsDialog();

    QString getUrl() const;
    QString getCurrentModel() const;
    QString getPrompt() const;

private slots:
    void updateList();

private:
    Ollama *m_ollama;
    QLineEdit *m_lineEdit;
    QComboBox *m_comboBox;
    QPlainTextEdit *m_plainTextEdit;
};

#endif // OLLAMASETTINGSDIALOG_H
