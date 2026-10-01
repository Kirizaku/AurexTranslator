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

#include "tesseractsettingsdialog.h"
#include "src/utils/logger.h"
#include "src/utils/dialogutils.h"

#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QButtonGroup>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QFileDialog>

TesseractSettingsDialog::TesseractSettingsDialog(const QString &status,
                                                const QString &currentLanguage,
                                                const QStringList &LanguageList,
                                                const QString &tessdataPath,
                                                bool useSystemTessdata,
                                                int mode,
                                                int autoInterval,
                                                TesseractOcr *tesseractOcr,
                                                QWidget *parent)
    : QDialog(parent)
{
    m_tesseractOcr = tesseractOcr;

    setWindowTitle("Tesseract");

    // Status
    QLabel *statusTitleLabel = new QLabel(tr("Status"));
    m_statusLabel = new QLabel(status);
    
    // Language
    QLabel *comboBoxLabel = new QLabel(tr("Language"));
    m_comboBox = new QComboBox;
    m_comboBox->addItems(LanguageList);

    int index = LanguageList.indexOf(currentLanguage);
    if (index != -1) {
        m_comboBox->setCurrentIndex(index);
    }

    QPushButton *updateButton = new QPushButton();
    updateButton->setText(tr("Update list"));
    connect(updateButton, &QPushButton::clicked, this, &TesseractSettingsDialog::on_updateLanguagesButton_clicked);

    QHBoxLayout *langLayout = new QHBoxLayout;
    langLayout->addWidget(m_comboBox);
    langLayout->addWidget(updateButton);

    // Tessdata path
    QLabel *pathLabel = new QLabel(tr("Tessdata path"));
    m_pathLineEdit = new QLineEdit;
    m_pathLineEdit->setText(tessdataPath);

    QPushButton *browseButton = new QPushButton(tr("Browse"));
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Tessdata Directory", m_pathLineEdit->text());
        if (!path.isEmpty()) {
            m_pathLineEdit->setText(path);
        }
    });

    QHBoxLayout *pathLayout = new QHBoxLayout;
    pathLayout->addWidget(m_pathLineEdit);
    pathLayout->addWidget(browseButton);

    // Use system tessdata checkbox
    m_systemTessdataCheckBox = new QCheckBox(tr("Use system tessdata"));
    m_systemTessdataCheckBox->setChecked(useSystemTessdata);

    // Processing mode
    QLabel *modeLabel = new QLabel(tr("Processing mode"));
    m_autoRadio = new QRadioButton(tr("Automatic"));
    m_manualRadio = new QRadioButton(tr("Manual"));
    
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->addButton(m_autoRadio, int(Mode::Auto));
    m_modeGroup->addButton(m_manualRadio, int(Mode::Manual));
    
    if (mode == 0) {
        m_autoRadio->setChecked(true);
    } else {
        m_manualRadio->setChecked(true);
    }

    QHBoxLayout *modeLayout = new QHBoxLayout;
    modeLayout->addWidget(m_autoRadio);
    modeLayout->addWidget(m_manualRadio);

    // Auto OCR interval
    QLabel *intervalLabel = new QLabel(tr("Auto OCR interval (sec)"));
    m_intervalSpinBox = new QSpinBox;
    m_intervalSpinBox->setRange(1, 3600);
    m_intervalSpinBox->setSuffix(" sec");
    m_intervalSpinBox->setValue(autoInterval);

    connect(m_manualRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_intervalSpinBox->setEnabled(!checked);
    });

    // Apply font styles
    QFont labelFont;
    labelFont.setBold(true);
    statusTitleLabel->setFont(labelFont);
    comboBoxLabel->setFont(labelFont);
    pathLabel->setFont(labelFont);
    modeLabel->setFont(labelFont);
    intervalLabel->setFont(labelFont);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Form layout
    QFormLayout *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow(statusTitleLabel, m_statusLabel);
    formLayout->addRow(comboBoxLabel, langLayout);
    formLayout->addRow(pathLabel, pathLayout);
    formLayout->addRow("", m_systemTessdataCheckBox);
    formLayout->addRow(modeLabel, modeLayout);
    formLayout->addRow(intervalLabel, m_intervalSpinBox);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
    adjustSize();
}

TesseractSettingsDialog::~TesseractSettingsDialog() {}

QString TesseractSettingsDialog::getCurrentLanguage() const
{
    return m_comboBox->currentText();
}

QStringList TesseractSettingsDialog::getLanguageList() const
{
    QStringList items;
    for (int i = 0; i < m_comboBox->count(); ++i) {
        items << m_comboBox->itemText(i);
    }
    return items;
}

QString TesseractSettingsDialog::getTessdataPath() const
{
    return m_pathLineEdit->text();
}

bool TesseractSettingsDialog::getUseSystemTessdata() const
{
    return m_systemTessdataCheckBox->isChecked();
}

int TesseractSettingsDialog::getMode() const
{
    return static_cast<Mode>(m_modeGroup->checkedId());
}

int TesseractSettingsDialog::getAutoInterval() const
{
    return m_intervalSpinBox->value();
}

void TesseractSettingsDialog::on_updateLanguagesButton_clicked()
{
    const bool wasRunning = m_tesseractOcr->isRunning();
    const QString originalPath = m_tesseractOcr->getTessdataPath();
    const QString originalLang = m_tesseractOcr->getLanguage();

    auto restoreState = [this, wasRunning, originalPath, originalLang]() {
        if (wasRunning) {
            m_tesseractOcr->setTessdataPath(originalPath);
            m_tesseractOcr->init(originalLang);
        }
    };

    if (wasRunning) {  
        m_tesseractOcr->stop();
    }

    m_comboBox->clear();
    m_tesseractOcr->setTessdataPath("");

    if (!m_systemTessdataCheckBox->isChecked()) {
        QString tessdataPath = m_pathLineEdit->text();
        QDir dir(tessdataPath);
        bool isValidTessdataDir = dir.exists() && !dir.entryList({"*.traineddata"}, QDir::Files).isEmpty();
        if (!isValidTessdataDir) {
            Log(Logger::Level::Warning, "[tesseract] The specified Tesseract data directory does not exist or is invalid");
            DialogUtils::warning(nullptr, tr("Invalid Tesseract Data Directory"),
                                 tr("The specified Tesseract data directory does not exist or is invalid.\n"
                                    "Please provide a valid path or try using the system default directory."));
            restoreState();
            return;
        }
        m_tesseractOcr->setTessdataPath(tessdataPath);
    }

    std::vector<std::string> languages = m_tesseractOcr->checkAvailableLanguages();

    if (languages.empty()) {
        Log(Logger::Level::Warning, "[tesseract] Tesseract could not find any language data in system locations");
        DialogUtils::information(nullptr, tr("No Tesseract available languages found"),
                                 tr("Tesseract could not find any language data in system locations.\n"
                                    "Please install Tesseract language packs or specify a custom 'tessdata' directory."));
        restoreState();
        return;
    }

    for (const auto& language : languages) {
        m_comboBox->addItem(QString::fromStdString(language));
    }

    restoreState();
}
