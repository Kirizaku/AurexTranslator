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
#include <QPushButton>
#include <QLabel>
#include "src/UI/widgets/processpickerwidget.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

HookSettingsDialog::HookSettingsDialog(HookMode currentMode,
                                       const QMap<QString, QString> &gameAppList,
                                       const QMap<QString, QString> &engineList,
                                       QString &currentGameAppPlugin,
                                       QString &currentEnginePlugin,
                                       QWidget *parent) : QDialog(parent)
{
    setWindowTitle("HOOK");
    resize(560, 540);

    m_gameAppRadio = new QRadioButton(tr("Game / Application"), this);
    m_engineRadio = new QRadioButton(tr("Engine"), this);
    m_gameAppRadio->setChecked(true);

    QHBoxLayout *radioLayout = new QHBoxLayout;
    radioLayout->addWidget(m_gameAppRadio);
    radioLayout->addWidget(m_engineRadio);
    radioLayout->addStretch();

    connect(m_gameAppRadio, &QRadioButton::toggled, this, &HookSettingsDialog::onModeChanged);
    connect(m_engineRadio, &QRadioButton::toggled, this, &HookSettingsDialog::onModeChanged);

    QWidget *gamePage = new QWidget(this);

    m_gameListComboBox = new QComboBox(gamePage);
    m_gameListComboBox->addItem(tr("— Select game/app —"), QString());
    for (auto it = gameAppList.constBegin(); it != gameAppList.constEnd(); ++it) {
        const QString displayText = QString("%1 (%2)").arg(it.value(), it.key());
        m_gameListComboBox->addItem(displayText, it.key());
    }
    const int currentGameAppPluginIndex = m_gameListComboBox->findData(currentGameAppPlugin);
    if (currentGameAppPluginIndex >= 0)
        m_gameListComboBox->setCurrentIndex(currentGameAppPluginIndex);

    QFormLayout *gameForm = new QFormLayout(gamePage);
    gameForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    gameForm->addRow(tr("Target:"), m_gameListComboBox);

    QWidget *enginePage = new QWidget(this);

    // Engine
    m_engineComboBox = new QComboBox(enginePage);
    m_engineComboBox->addItem(tr("— Select engine —"), QString());

    for (auto it = engineList.constBegin(); it != engineList.constEnd(); ++it) {
        const QString displayText = QString("%1 (%2)").arg(it.value(), it.key());
        m_engineComboBox->addItem(displayText, it.key());
    }
    const int currentEnginePluginIndex = m_engineComboBox->findData(currentEnginePlugin);
    if (currentEnginePluginIndex >= 0)
        m_engineComboBox->setCurrentIndex(currentEnginePluginIndex);

    QFormLayout *engineForm = new QFormLayout;
    engineForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    engineForm->addRow(tr("Engine:"), m_engineComboBox);

    QGroupBox *processBox = new QGroupBox(tr("Process"), enginePage);

    m_processPicker = new ProcessPickerWidget(processBox);

    QVBoxLayout *processLayout = new QVBoxLayout(processBox);
    processLayout->addWidget(m_processPicker, 1);

    processBox->setEnabled(false);
    processBox->setObjectName("processBox");

    QVBoxLayout *m_engineLayout = new QVBoxLayout(enginePage);
    m_engineLayout->addLayout(engineForm);
    m_engineLayout->addWidget(processBox, 1);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(gamePage);
    m_stack->addWidget(enginePage);

    connect(m_engineComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HookSettingsDialog::onEngineChanged);

    onEngineChanged();

    if (currentMode == EngineMode)
        m_engineRadio->setChecked(true);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(radioLayout);

    mainLayout->addWidget(m_stack, 1);
#ifdef Q_OS_LINUX
    m_ptraceLabel = new QLabel(tr("Injection will ask for a password every time. To turn the prompt off, click \"Allow without password\" and enter your password once."), this);
    m_ptraceLabel->setWordWrap(true);
    m_ptraceLabel->hide();
    mainLayout->addWidget(m_ptraceLabel);

    m_ptraceButton = new QPushButton(tr("Allow without password"), this);
    m_ptraceButton->hide();
    connect(m_ptraceButton, &QPushButton::clicked, this, &HookSettingsDialog::ptraceGrantRequested);
#endif

    QHBoxLayout *bottomLayout = new QHBoxLayout;
#ifdef Q_OS_LINUX
    bottomLayout->addWidget(m_ptraceButton);
#endif
    bottomLayout->addStretch();
    bottomLayout->addWidget(buttonBox);
    mainLayout->addLayout(bottomLayout);
    setLayout(mainLayout);

    onModeChanged();
}

HookSettingsDialog::~HookSettingsDialog() {}

#ifdef Q_OS_LINUX
void HookSettingsDialog::setPtraceStatus(const QString &status)
{
    const bool missing = status == QLatin1String("missing");
    m_ptraceLabel->setVisible(missing);
    m_ptraceButton->setVisible(missing);
}
#endif

HookSettingsDialog::HookMode HookSettingsDialog::getCurrentMode() const
{
    return m_engineRadio->isChecked() ? EngineMode : GameAppMode;
}

QString HookSettingsDialog::getCurrentGameAppPlugin() const
{
    return m_gameListComboBox->currentData().toString();
}

QString HookSettingsDialog::getSelectedEngine() const
{
    return m_engineComboBox->currentData().toString();
}

QString HookSettingsDialog::getSelectedProcessName() const
{
    return m_processPicker->selectedProcessName();
}

void HookSettingsDialog::setEngineList(const QStringList &engines, const QString &current)
{
    m_engineComboBox->blockSignals(true);
    m_engineComboBox->clear();
    m_engineComboBox->addItem(tr("— Select engine —"), QString());
    for (const QString &e : engines)
        m_engineComboBox->addItem(e, e);

    if (!current.isEmpty()) {
        const int idx = m_engineComboBox->findData(current);
        if (idx >= 0)
            m_engineComboBox->setCurrentIndex(idx);
    }
    m_engineComboBox->blockSignals(false);
    onEngineChanged();
}

void HookSettingsDialog::onModeChanged()
{
    if (m_engineRadio->isChecked()) {
        m_stack->setCurrentIndex(1);
    } else {
        m_stack->setCurrentIndex(0);
    }
}

void HookSettingsDialog::onEngineChanged()
{
    const bool hasEngine = !getSelectedEngine().isEmpty();

    if (auto *box = m_stack->widget(1)->findChild<QGroupBox *>("processBox"))
        box->setEnabled(hasEngine);

    if (hasEngine) {
        if (!m_processesLoaded) {
            m_processPicker->refresh();
            m_processesLoaded = true;
        }
    }
}