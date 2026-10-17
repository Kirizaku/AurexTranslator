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

#include "processpickerwidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <QDir>
#endif

ProcessPickerWidget::ProcessPickerWidget(QWidget *parent) : QWidget(parent)
{
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter by PID or process name..."));
    m_filterEdit->setClearButtonEnabled(true);

    m_refreshButton = new QPushButton(tr("Refresh"), this);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(m_filterEdit, 1);
    filterLayout->addWidget(m_refreshButton);

    m_model = new QStandardItemModel(0, 2, this);
    m_model->setHorizontalHeaderLabels({tr("PID"), tr("Process name")});

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    m_view = new QTreeView(this);
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSortingEnabled(true);
    m_view->setUniformRowHeights(true);
    m_view->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_view->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_view->sortByColumn(1, Qt::AscendingOrder);

    connect(m_refreshButton, &QPushButton::clicked, this, &ProcessPickerWidget::refresh);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ProcessPickerWidget::onFilterTextChanged);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ProcessPickerWidget::selectionChanged);
    connect(m_view, &QTreeView::activated, this, &ProcessPickerWidget::activated);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(filterLayout);
    layout->addWidget(m_view, 1);
}

void ProcessPickerWidget::refresh()
{
    m_model->removeRows(0, m_model->rowCount());

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

            m_model->appendRow({pidItem, nameItem});
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

        m_model->appendRow({pidItem, nameItem});
    }
#endif

    onFilterTextChanged(m_filterEdit->text());
}

QString ProcessPickerWidget::selectedProcessName() const
{
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid())
        return QString();

    const QModelIndex nameIdx = m_proxy->index(idx.row(), 1, idx.parent());
    return m_proxy->data(nameIdx, Qt::DisplayRole).toString();
}

void ProcessPickerWidget::onFilterTextChanged(const QString &text)
{
    m_proxy->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(text),
                                                           QRegularExpression::CaseInsensitiveOption));
}
