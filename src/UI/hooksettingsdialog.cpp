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
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <QDir>
#include <QFile>
#endif

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

    m_searchEdit = new QLineEdit(processBox);
    m_searchEdit->setPlaceholderText(tr("Filter by PID or process name..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_refreshButton = new QPushButton(tr("Refresh"), processBox);

    QHBoxLayout *searchLayout = new QHBoxLayout;
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_refreshButton);

    m_processModel = new QStandardItemModel(0, 2, this);
    m_processModel->setHorizontalHeaderLabels({("PID"), tr("Process name")});

    m_processProxy = new QSortFilterProxyModel(this);
    m_processProxy->setSourceModel(m_processModel);
    m_processProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_processProxy->setFilterKeyColumn(-1);

    m_processView = new QTreeView(processBox);
    m_processView->setModel(m_processProxy);
    m_processView->setRootIsDecorated(false);
    m_processView->setAlternatingRowColors(true);
    m_processView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_processView->setSortingEnabled(true);
    m_processView->setUniformRowHeights(true);
    m_processView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_processView->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_processView->sortByColumn(1, Qt::AscendingOrder);

    QVBoxLayout *processLayout = new QVBoxLayout(processBox);
    processLayout->addLayout(searchLayout);
    processLayout->addWidget(m_processView, 1);

    connect(m_refreshButton, &QPushButton::clicked, this, &HookSettingsDialog::onRefreshProcesses);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HookSettingsDialog::onFilterTextChanged);

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

    onEngineChanged(m_engineComboBox->currentIndex());

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
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    onModeChanged();
}

HookSettingsDialog::~HookSettingsDialog() {}

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
    const QModelIndex idx = m_processView->currentIndex();
    if (!idx.isValid())
        return QString();
    const QModelIndex nameIdx = m_processProxy->index(idx.row(), 1, idx.parent());
    return m_processProxy->data(nameIdx, Qt::DisplayRole).toString();
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
    onEngineChanged(m_engineComboBox->currentIndex());
}

void HookSettingsDialog::onModeChanged()
{
    if (m_engineRadio->isChecked()) {
        m_stack->setCurrentIndex(1);
    } else {
        m_stack->setCurrentIndex(0);
    }
}

void HookSettingsDialog::onEngineChanged(int /*index*/)
{
    const bool hasEngine = !getSelectedEngine().isEmpty();

    if (auto *box = m_stack->widget(1)->findChild<QGroupBox *>("processBox"))
        box->setEnabled(hasEngine);

    if (hasEngine) {
        if (m_processModel->rowCount() == 0)
            loadProcesses();
    } else {
        m_processModel->removeRows(0, m_processModel->rowCount());
    }
}

void HookSettingsDialog::onRefreshProcesses()
{
    loadProcesses();
}

void HookSettingsDialog::onFilterTextChanged(const QString &text)
{
    m_processProxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
}

void HookSettingsDialog::loadProcesses()
{
    m_processModel->removeRows(0, m_processModel->rowCount());

#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            auto *pidItem  = new QStandardItem;
            auto *nameItem = new QStandardItem;

            const qint64 pid = static_cast<qint64>(entry.th32ProcessID);
            pidItem->setData(pid, Qt::EditRole);
            pidItem->setData(pid, Qt::DisplayRole);
            pidItem->setEditable(false);
            pidItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            nameItem->setText(QString::fromWCharArray(entry.szExeFile));
            nameItem->setEditable(false);

            m_processModel->appendRow({pidItem, nameItem});
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
#else
    QDir proc("/proc");
    const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dirName : entries) {
        bool ok = false;
        const qint64 pid = dirName.toLongLong(&ok);
        if (!ok)
            continue;

        QString name;
        QFile commFile(QString("/proc/%1/comm").arg(pid));
        if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            name = QString::fromUtf8(commFile.readAll()).trimmed();
        }
        if (name.isEmpty())
            continue;

        auto *pidItem  = new QStandardItem;
        auto *nameItem = new QStandardItem(name);

        pidItem->setData(pid, Qt::EditRole);
        pidItem->setData(pid, Qt::DisplayRole);
        pidItem->setEditable(false);
        pidItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nameItem->setEditable(false);

        m_processModel->appendRow({pidItem, nameItem});
    }
#endif

    onFilterTextChanged(m_searchEdit->text());
}