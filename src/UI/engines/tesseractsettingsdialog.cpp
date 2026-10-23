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
#include "ui_tesseractsettingsdialog.h"
#include "src/utils/logger.h"
#include "src/utils/dialogutils.h"

#include <QDir>
#include <QFileDialog>

TesseractSettingsDialog::TesseractSettingsDialog(const QString &status,
                                                const TesseractConfig &config,
                                                TesseractOcr *tesseractOcr,
                                                QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TesseractSettingsDialog)
    , m_tesseractOcr(tesseractOcr)
{
    ui->setupUi(this);

    ui->applyHintLabel->setForegroundRole(QPalette::PlaceholderText);

    ui->splitter->setCollapsible(0, false);
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes({480, 420});

    ui->psmComboBox->addItem(tr("Auto (by region size)"), TesseractTuning::PsmAuto);
    ui->psmComboBox->addItem(tr("Single block"), TesseractTuning::PsmBlock);
    ui->psmComboBox->addItem(tr("Single line"), TesseractTuning::PsmLine);
    ui->psmComboBox->addItem(tr("Sparse text"), TesseractTuning::PsmSparse);

    connect(ui->browseButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Tessdata Directory", ui->tessdataPathEdit->text());
        if (!path.isEmpty()) {
            ui->tessdataPathEdit->setText(path);
        }
    });

    connect(ui->resetButton, &QPushButton::clicked, this, [this]() {
        const TesseractTuning defaults;
        ui->skipUnchangedCheckBox->setChecked(defaults.skipUnchanged);
        ui->filterNoiseCheckBox->setChecked(defaults.filterNoise);
        ui->sensitivitySlider->setValue(defaults.changeSensitivity);
        ui->toleranceSlider->setValue(defaults.textTolerance);
        ui->confidenceSlider->setValue(defaults.minConfidence);
        ui->psmComboBox->setCurrentIndex(ui->psmComboBox->findData(defaults.psm));
    });

    connect(ui->sensitivitySlider, &QSlider::valueChanged, this, &TesseractSettingsDialog::updateValueLabels);
    connect(ui->toleranceSlider, &QSlider::valueChanged, this, &TesseractSettingsDialog::updateValueLabels);
    connect(ui->confidenceSlider, &QSlider::valueChanged, this, &TesseractSettingsDialog::updateValueLabels);

    connect(ui->manualRadio, &QRadioButton::toggled, this, &TesseractSettingsDialog::updateEnabledState);
    connect(ui->skipUnchangedCheckBox, &QCheckBox::toggled, this, &TesseractSettingsDialog::updateEnabledState);
    connect(ui->filterNoiseCheckBox, &QCheckBox::toggled, this, &TesseractSettingsDialog::updateEnabledState);

    auto applyLive = [this]() {
        if (!m_loading) m_tesseractOcr->setTuning(currentTuning());
    };
    
    connect(ui->skipUnchangedCheckBox, &QCheckBox::toggled, this, applyLive);
    connect(ui->filterNoiseCheckBox, &QCheckBox::toggled, this, applyLive);
    connect(ui->sensitivitySlider, &QSlider::valueChanged, this, applyLive);
    connect(ui->toleranceSlider, &QSlider::valueChanged, this, applyLive);
    connect(ui->confidenceSlider, &QSlider::valueChanged, this, applyLive);
    connect(ui->psmComboBox, &QComboBox::currentIndexChanged, this, applyLive);

    // Apply is available only while the dialog differs from what is currently applied
    auto onChanged = [this]() {
        if (!m_loading) updateApplyButton();
    };
    connect(ui->languageComboBox, &QComboBox::currentIndexChanged, this, onChanged);
    connect(ui->tessdataPathEdit, &QLineEdit::textChanged, this, onChanged);
    connect(ui->systemTessdataCheckBox, &QCheckBox::toggled, this, onChanged);
    connect(ui->manualRadio, &QRadioButton::toggled, this, onChanged);
    connect(ui->intervalSpinBox, &QSpinBox::valueChanged, this, onChanged);
    connect(ui->skipUnchangedCheckBox, &QCheckBox::toggled, this, onChanged);
    connect(ui->filterNoiseCheckBox, &QCheckBox::toggled, this, onChanged);
    connect(ui->sensitivitySlider, &QSlider::valueChanged, this, onChanged);
    connect(ui->toleranceSlider, &QSlider::valueChanged, this, onChanged);
    connect(ui->confidenceSlider, &QSlider::valueChanged, this, onChanged);
    connect(ui->psmComboBox, &QComboBox::currentIndexChanged, this, onChanged);

    connect(m_tesseractOcr, &TesseractOcr::statsUpdated, this, [this](int runs, int skipped, int duplicates, int sent) {
        ui->previewHintLabel->hide();
        ui->readValueLabel->setText(QString::number(runs));
        ui->skippedUnchangedValueLabel->setText(QString::number(skipped));
        ui->skippedSameValueLabel->setText(QString::number(duplicates));
        ui->sentValueLabel->setText(QString::number(sent));
    });
    connect(m_tesseractOcr, &TesseractOcr::recognized, this, [this](const QString &text, const QString &dropped) {
        ui->recognizedEdit->setPlainText(text);
        ui->droppedEdit->setPlainText(dropped);
    });
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {
        if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole) emit applyRequested();
    });
    connect(this, &QDialog::rejected, this, [this]() { m_tesseractOcr->setTuning(m_originalConfig.tuning); });

    setValues(status, config);
}

TesseractSettingsDialog::~TesseractSettingsDialog()
{
    delete ui;
}

void TesseractSettingsDialog::setValues(const QString &status, const TesseractConfig &config)
{
    m_loading = true;
    m_originalConfig = config;

    ui->statusLabel->setText(status);

    ui->languageComboBox->clear();
    ui->languageComboBox->addItems(config.languages);
    const int index = config.languages.indexOf(config.language);
    if (index != -1) {
        ui->languageComboBox->setCurrentIndex(index);
    }

    ui->tessdataPathEdit->setText(config.tessdataPath);
    ui->systemTessdataCheckBox->setChecked(config.useSystemTessdata);
    (config.mode == Auto ? ui->autoRadio : ui->manualRadio)->setChecked(true);
    ui->intervalSpinBox->setValue(config.autoInterval);

    const TesseractTuning &tuning = config.tuning;
    ui->skipUnchangedCheckBox->setChecked(tuning.skipUnchanged);
    ui->filterNoiseCheckBox->setChecked(tuning.filterNoise);
    ui->sensitivitySlider->setValue(tuning.changeSensitivity);
    ui->toleranceSlider->setValue(tuning.textTolerance);
    ui->confidenceSlider->setValue(tuning.minConfidence);
    ui->psmComboBox->setCurrentIndex(std::max(0, ui->psmComboBox->findData(tuning.psm)));
    updateValueLabels();

    ui->previewHintLabel->setText(tr("Waiting for frames..."));
    ui->previewHintLabel->show();
    for (QLabel *counter : {ui->readValueLabel, ui->skippedUnchangedValueLabel, ui->skippedSameValueLabel, ui->sentValueLabel}) {
        counter->setText(QStringLiteral("0"));
    }
    ui->recognizedEdit->clear();
    ui->droppedEdit->clear();

    m_loading = false;
    updateEnabledState();
    updateApplyButton();
    m_tesseractOcr->resetStats();
}

TesseractConfig TesseractSettingsDialog::getConfig() const
{
    TesseractConfig config;
    config.language = ui->languageComboBox->currentText();
    for (int i = 0; i < ui->languageComboBox->count(); ++i) {
        config.languages << ui->languageComboBox->itemText(i);
    }
    config.tessdataPath = ui->tessdataPathEdit->text();
    config.useSystemTessdata = ui->systemTessdataCheckBox->isChecked();
    config.mode = ui->manualRadio->isChecked() ? Manual : Auto;
    config.autoInterval = ui->intervalSpinBox->value();
    config.tuning = currentTuning();
    return config;
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

    ui->languageComboBox->clear();
    m_tesseractOcr->setTessdataPath("");

    if (!ui->systemTessdataCheckBox->isChecked()) {
        QString tessdataPath = ui->tessdataPathEdit->text();
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
        ui->languageComboBox->addItem(QString::fromStdString(language));
    }

    restoreState();
}

TesseractTuning TesseractSettingsDialog::currentTuning() const
{
    TesseractTuning tuning;
    tuning.skipUnchanged = ui->skipUnchangedCheckBox->isChecked();
    tuning.filterNoise = ui->filterNoiseCheckBox->isChecked();
    tuning.changeSensitivity = ui->sensitivitySlider->value();
    tuning.textTolerance = ui->toleranceSlider->value();
    tuning.minConfidence = ui->confidenceSlider->value();
    tuning.psm = ui->psmComboBox->currentData().toInt();
    return tuning;
}

void TesseractSettingsDialog::updateValueLabels()
{
    ui->sensitivityValueLabel->setText(QString::number(ui->sensitivitySlider->value()));
    ui->toleranceValueLabel->setText(tr("%1 chars").arg(ui->toleranceSlider->value()));
    ui->confidenceValueLabel->setText(QString::number(ui->confidenceSlider->value()));
}

void TesseractSettingsDialog::updateEnabledState()
{
    const bool manual = ui->manualRadio->isChecked();
    const bool skip = ui->skipUnchangedCheckBox->isChecked();
    ui->intervalSpinBox->setEnabled(!manual);
    ui->sensitivitySlider->setEnabled(!manual && skip);
    ui->toleranceSlider->setEnabled(skip);
    ui->confidenceSlider->setEnabled(ui->filterNoiseCheckBox->isChecked());
}

void TesseractSettingsDialog::updateApplyButton()
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(!getConfig().sameSettings(m_originalConfig));
}
