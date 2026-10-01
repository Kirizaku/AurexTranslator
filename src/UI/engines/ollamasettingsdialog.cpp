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
#include "src/utils/dialogutils.h"

#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QButtonGroup>

OllamaSettingsDialog::OllamaSettingsDialog(Ollama *ollama, QString &currentModel, QJsonArray &models, QWidget *parent) : QDialog(parent)
{
    m_ollama = ollama;

    setWindowTitle("Ollama");
    setMinimumSize(600, 500);

    // URL
    QLabel *lineEditLabel = new QLabel("Url");
    m_lineEdit = new QLineEdit;

    // Model
    QLabel *comboBoxLabel = new QLabel(tr("Model"));
    m_comboBox = new QComboBox;

    for (const QJsonValue &value : models) {
        if (value.isString()) {
            m_comboBox->addItem(value.toString());
        }
    }

    // Update list
    updateButton = new QPushButton();
    updateButton->setText(tr("Update list"));
    connect(updateButton, &QPushButton::clicked, this, &OllamaSettingsDialog::updateList);

    QHBoxLayout *hBoxLayout = new QHBoxLayout;
    hBoxLayout->addWidget(m_comboBox);
    hBoxLayout->addWidget(updateButton);

    // Translation Prompt
    QLabel *translationPromptLabel = new QLabel(tr("Translation Prompt"));
    m_translationPromptEdit = new QPlainTextEdit;
    m_translationPromptEdit->setPlaceholderText(tr("Translate the following text from Japanese to English. "
                                              "Return **only the translated text** - no explanations, notes, formatting, or original text. "
                                              "Do not add anything else. Text:"));

    // Vision Prompt
    QLabel *visionPromptLabel = new QLabel(tr("Vision Prompt"));
    m_visionPromptEdit = new QPlainTextEdit;
    m_visionPromptEdit->setPlaceholderText(tr("Analyze the image and tell me what text is shown on it. "
                                              "Then, return **only the extracted text** - no explanations, comments, labels, or extra information. "
                                              "Do not add anything else."));

    // Vision Mode
    QLabel *modeLabel = new QLabel(tr("Vision Mode"));
    m_autoRadio = new QRadioButton(tr("Automatic"));
    m_manualRadio = new QRadioButton(tr("Manual"));

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->addButton(m_autoRadio, int(Mode::Auto));
    m_modeGroup->addButton(m_manualRadio, int(Mode::Manual));

    QHBoxLayout *modeLayout = new QHBoxLayout;
    modeLayout->addWidget(m_autoRadio);
    modeLayout->addWidget(m_manualRadio);

    m_ollamaWaitResponseCheckBox = new QCheckBox(tr("Wait for response"));

    // Auto OCR interval
    QLabel *intervalLabel = new QLabel(tr("Auto OCR interval (sec)"));
    m_intervalSpinBox = new QSpinBox;
    m_intervalSpinBox->setRange(1, 3600);
    m_intervalSpinBox->setSuffix(" sec");

    connect(m_manualRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_ollamaWaitResponseCheckBox->setEnabled(!checked);
        m_intervalSpinBox->setEnabled(!checked);
    });

    // Warning: Automatic mode
    QLabel *gpuWarningLabel = new QLabel(tr("Warning: Automatic mode heavily utilizes the GPU and may cause high system load. Enabling \"Wait for response\" reduces GPU load significantly."));
    gpuWarningLabel->setWordWrap(true);
    gpuWarningLabel->setStyleSheet("color: grey; font-size: 10pt;");

    // Apply font styles
    QFont labelFont;
    labelFont.setBold(true);
    lineEditLabel->setFont(labelFont);
    comboBoxLabel->setFont(labelFont);
    translationPromptLabel->setFont(labelFont);
    visionPromptLabel->setFont(labelFont);
    modeLabel->setFont(labelFont);
    intervalLabel->setFont(labelFont);
    gpuWarningLabel->setFont(labelFont);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Main layout
    QFormLayout *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow(lineEditLabel, m_lineEdit);
    formLayout->addRow(comboBoxLabel, hBoxLayout);
    formLayout->addRow(translationPromptLabel, m_translationPromptEdit);
    formLayout->addRow(visionPromptLabel, m_visionPromptEdit);
    formLayout->addRow(modeLabel, modeLayout);
    formLayout->addRow(intervalLabel, m_intervalSpinBox);
    formLayout->addRow(nullptr, m_ollamaWaitResponseCheckBox);
    formLayout->addRow(gpuWarningLabel);
    formLayout->addRow(buttonBox);

    setLayout(formLayout);
}

OllamaSettingsDialog::~OllamaSettingsDialog() {}

void OllamaSettingsDialog::updateList()
{
    updateButton->setEnabled(false);

    m_ollama->checkServerAvailable(QUrl(m_lineEdit->text()), [this](bool isAvailable) {
        if (!isAvailable) {
            Log(Logger::Level::Warning, "[ollama] Server is unavailable");
            DialogUtils::warning(this, tr("Server Unavailable"),
                                 tr("Ollama server is unavailable. Please check if the server is running and the URL is correct."));
            updateButton->setEnabled(true);
            return;
        }

        m_ollama->checkModelsAvailable(QUrl(m_lineEdit->text()), [this](const QStringList& models) {
            if (models.isEmpty()) {
                Log(Logger::Level::Warning, "[ollama] Failed to load models or list is empty");
                DialogUtils::warning(this, tr("No Models Found"),
                                     tr("Failed to load models or the model list is empty. Please check if models are installed on the server."));
            } else {
                m_comboBox->clear();
                m_comboBox->addItems(models);
            }
            updateButton->setEnabled(true);
        });
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

QJsonArray OllamaSettingsDialog::getModels() const
{
    QJsonArray models;
    for (int i = 0; i < m_comboBox->count(); ++i) {
        models.append(m_comboBox->itemText(i));
    }
    return models;
}

QString OllamaSettingsDialog::getTranslationPrompt() const
{
    return m_translationPromptEdit->toPlainText();
}

QString OllamaSettingsDialog::getVisionPrompt() const
{
    return m_visionPromptEdit->toPlainText();
}

int OllamaSettingsDialog::getMode() const
{
    return static_cast<Mode>(m_modeGroup->checkedId());
}

int OllamaSettingsDialog::getAutoInterval() const
{
    return m_intervalSpinBox->value();
}

bool OllamaSettingsDialog::getIsWaitForResponse() const
{
    return m_ollamaWaitResponseCheckBox->isChecked();
}

void OllamaSettingsDialog::setCurrentSettings(const QString &url, QString &currentModel, const QString &prompt, const QString &visionPrompt, const int &visionMode, const int &visionAutoInterval, const bool &waitForOllamaResponse)
{
    m_lineEdit->setText(url);
    m_translationPromptEdit->setPlainText(prompt);
    m_visionPromptEdit->setPlainText(visionPrompt);

    if (visionMode == 0) {
        m_autoRadio->setChecked(true);
    } else {
        m_manualRadio->setChecked(true);
    }

    m_intervalSpinBox->setValue(visionAutoInterval);
    m_ollamaWaitResponseCheckBox->setChecked(waitForOllamaResponse);

    int currentModelIndex = m_comboBox->findText(currentModel);
    if (currentModelIndex >= 0) {
        m_comboBox->setCurrentIndex(currentModelIndex);
    }
}
