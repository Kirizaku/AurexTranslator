/******************************************************************************
    Copyright (C) 2026 by Daniil Nabiulin

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

#include "hooksettingsdialog.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

HookSettingsDialog::HookSettingsDialog(const QMap<QString, QString> &gameList, QString &currentPlugin, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("HOOK");

    // Game selection
    QLabel *programLabel = new QLabel(tr("Game"));
    gameListComboBox = new QComboBox;

    for (auto it = gameList.constBegin(); it != gameList.constEnd(); ++it) {
        QString displayText = QString("%1 (%2)").arg(it.value()).arg(it.key());
        gameListComboBox->addItem(displayText, it.key());
    }

    int currentPluginIndex = gameListComboBox->findData(currentPlugin);
    if (currentPluginIndex >= 0) {
        gameListComboBox->setCurrentIndex(currentPluginIndex);
    }

    // Apply font styles
    QFont labelFont;
    labelFont.setBold(true);
    programLabel->setFont(labelFont);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Form layout
    QFormLayout *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow(programLabel, gameListComboBox);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
    adjustSize();
}

HookSettingsDialog::~HookSettingsDialog() {}

QString HookSettingsDialog::getCurrentNamePlugin() const
{
    return gameListComboBox->currentData().toString();
}
