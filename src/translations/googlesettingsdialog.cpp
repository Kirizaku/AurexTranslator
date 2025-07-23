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

#include <QLabel>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "googlesettingsdialog.h"

GoogleSettingsDialog::GoogleSettingsDialog(const QString &sourceLang, const QString &targetLang, Google *google, QWidget *parent)
{
    m_google = google;

    setWindowTitle("Google");

    QLabel *sourceLabel = new QLabel(tr("Source language"));
    sourceComboBox = new QComboBox;

    QLabel *targetLabel = new QLabel(tr("Target language"));
    targetComboBox = new QComboBox;

    for (auto it = languages.constBegin(); it != languages.constEnd(); ++it) {
        QString displayText = QString("%1 (%2)").arg(it.value()).arg(it.key());
        sourceComboBox->addItem(displayText, it.key());
        targetComboBox->addItem(displayText, it.key());
    }

    int sourceIndex = sourceComboBox->findData(sourceLang);
    if (sourceIndex >= 0) {
        sourceComboBox->setCurrentIndex(sourceIndex);
    }

    int targetIndex = targetComboBox->findData(targetLang);
    if (targetIndex >= 0) {
        targetComboBox->setCurrentIndex(targetIndex);
    }

    QFont labelFont;
    labelFont.setBold(true);
    sourceLabel->setFont(labelFont);
    targetLabel->setFont(labelFont);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow(sourceLabel, sourceComboBox);
    formLayout->addRow(targetLabel, targetComboBox);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
    adjustSize();
}

GoogleSettingsDialog::~GoogleSettingsDialog() {}

QString GoogleSettingsDialog::getSourceLang() const
{
    return sourceComboBox->currentData().toString();
}

QString GoogleSettingsDialog::getTargetLang() const
{
    return targetComboBox->currentData().toString();
}
