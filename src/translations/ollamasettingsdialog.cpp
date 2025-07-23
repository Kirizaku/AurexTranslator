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

#include "ollamasettingsdialog.h"
#include "src/utils/logger.h"

#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>

OllamaSettingsDialog::OllamaSettingsDialog(const QString &url, const QString &currentModel, const QStringList &models, const QString &prompt, Ollama *ollama, QWidget *parent)
{
    m_ollama = ollama;

    setWindowTitle("Ollama");
    setMinimumSize(400, 300);

    QLabel *lineEditLabel = new QLabel("Url");
    m_lineEdit = new QLineEdit;
    m_lineEdit->setText(url);

    QLabel *comboBoxLabel = new QLabel(tr("Model"));
    m_comboBox = new QComboBox;
    m_comboBox->addItems(models);

    int currentModelIndex = m_comboBox->findText(currentModel);
    if (currentModelIndex >= 0) {
        m_comboBox->setCurrentIndex(currentModelIndex);
    }

    QPushButton *updateButton = new QPushButton();
    updateButton->setText(tr("Update list"));
    connect(updateButton, &QPushButton::clicked, this, &OllamaSettingsDialog::updateList);

    QHBoxLayout *hBoxLayout = new QHBoxLayout;
    hBoxLayout->addWidget(m_comboBox);
    hBoxLayout->addWidget(updateButton);

    QLabel *plainTextLabel = new QLabel(tr("Prompt"));
    m_plainTextEdit = new QPlainTextEdit;
    m_plainTextEdit->setPlainText(prompt);

    QFont labelFont;
    labelFont.setBold(true);
    lineEditLabel->setFont(labelFont);
    comboBoxLabel->setFont(labelFont);
    plainTextLabel->setFont(labelFont);

    QFormLayout *formLayout = new QFormLayout;

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow(lineEditLabel, m_lineEdit);
    formLayout->addRow(comboBoxLabel, hBoxLayout);
    formLayout->addRow(plainTextLabel, m_plainTextEdit);
    formLayout->addRow(buttonBox);

    setLayout(formLayout);
}

OllamaSettingsDialog::~OllamaSettingsDialog() {}

void OllamaSettingsDialog::updateList()
{
    m_comboBox->clear();

    m_ollama->checkServerAvailable(QUrl(m_lineEdit->text()), [this](bool isAvailable) {
        if (!isAvailable) {
            Log(Logger::Level::Warning, "[ollama] Server is unavailable");
        } else {
            Log(Logger::Level::Info, "[ollama] Server is available");
            m_ollama->checkModelsAvailable([this](QStringList models) {
                if (models.isEmpty()) {
                    Log(Logger::Level::Warning, "[ollama] Failed to load models or list is empty");
                    return;
                }
                m_comboBox->addItems(models);
            });
        }
    });
}

QString OllamaSettingsDialog::getUrl() const
{
    return m_lineEdit->text();
}

QString OllamaSettingsDialog::getCurrentModel() const
{
    return m_comboBox->currentText();
}

QString OllamaSettingsDialog::getPrompt() const
{
    return m_plainTextEdit->toPlainText();
}
