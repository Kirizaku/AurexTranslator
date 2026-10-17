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

#include "overlaygameswitcherdialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include "overlayprocesspickerdialog.h"
#include <QMessageBox>
#endif

OverlayGameSwitcherDialog::OverlayGameSwitcherDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Switch Game"));

    m_preview = new QLabel(this);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setWordWrap(true);
    m_preview->setMinimumHeight(180);
    m_preview->setStyleSheet(QStringLiteral("QLabel { background: black; color: white; }"));
    m_preview->setText(tr("No active game"));

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_list, &QListWidget::currentItemChanged, this, &OverlayGameSwitcherDialog::onCurrentItemChanged);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    auto *label = new QLabel(tr("Select a game:"), this);
    label->setWordWrap(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_preview, 1);
    mainLayout->addWidget(label);
    mainLayout->addWidget(m_list, 1);

#ifdef Q_OS_WIN
    auto *addButton = buttons->addButton(tr("Add game..."), QDialogButtonBox::ActionRole);
    connect(addButton, &QPushButton::clicked, this, &OverlayGameSwitcherDialog::onAddClicked);

    m_removeButton = buttons->addButton(tr("Remove"), QDialogButtonBox::ActionRole);
    m_removeButton->setEnabled(false);
    connect(m_removeButton, &QPushButton::clicked, this, &OverlayGameSwitcherDialog::onRemoveClicked);
#else
    auto *launchHelpButton = buttons->addButton(tr("How do I launch a game?"), QDialogButtonBox::HelpRole);
    connect(launchHelpButton, &QPushButton::clicked, this, &OverlayGameSwitcherDialog::launchHelpRequested);
#endif

    mainLayout->addWidget(buttons);

    resize(420, 512);
    rebuildList();
}

void OverlayGameSwitcherDialog::setGameList(const QStringList &exeNames, const QList<quint32> &pids)
{
    m_connectedGames = exeNames;
    m_connectedPids = pids;
    rebuildList();
}

#ifdef Q_OS_WIN
void OverlayGameSwitcherDialog::setManagedGames(const QStringList &searchingGames,
                                                 const QStringList &injectingGames)
{
    m_searchingGames = searchingGames;
    m_injectingGames = injectingGames;
    m_managedExe = !searchingGames.isEmpty() ? searchingGames.first()
                 : !injectingGames.isEmpty() ? injectingGames.first()
                 : QString();
    rebuildList();
}

void OverlayGameSwitcherDialog::setKnownGames(const QStringList &otherGames)
{
    m_otherKnownGames = otherGames;
    rebuildList();
}
#endif

void OverlayGameSwitcherDialog::setActiveGame(const QString &exe, quint32 pid)
{
    m_activeExe = exe;
    m_activePid = pid;

    if (exe.isEmpty()) {
        clearPreview();
        return;
    }

    m_updatingProgrammatically = true;
    selectRow(exe, pid);
    m_updatingProgrammatically = false;
}

void OverlayGameSwitcherDialog::setPreviewFrame(const QImage &frame)
{
    if (frame.isNull() || m_activeExe.isEmpty()) return;
    m_lastFrame = frame.copy();
    m_lastFrame.setDevicePixelRatio(devicePixelRatio());
    rescalePreview();
}

void OverlayGameSwitcherDialog::clearPreview()
{
    setPreviewMessage(tr("No active game"));
}

void OverlayGameSwitcherDialog::setPreviewMessage(const QString &message)
{
    m_lastFrame = QImage();
    m_preview->clear();
    m_preview->setText(message);
}

void OverlayGameSwitcherDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    rescalePreview();
}

void OverlayGameSwitcherDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    emit switcherShown();
}

void OverlayGameSwitcherDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    emit switcherHidden();
}

void OverlayGameSwitcherDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    const QString exe = current ? current->data(Qt::UserRole).toString() : QString();
#ifdef Q_OS_WIN
    m_removeButton->setEnabled(!exe.isEmpty());
#endif
    if (m_updatingProgrammatically || exe.isEmpty()) return;
    emit gameSelected(exe, currentSelectedPid());
#ifdef Q_OS_WIN
    emit gameSwitchRequested(exe);
#endif
}

void OverlayGameSwitcherDialog::rescalePreview()
{
    if (m_lastFrame.isNull()) return;
    m_preview->setPixmap(QPixmap::fromImage(m_lastFrame).scaled(m_preview->size() * devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void OverlayGameSwitcherDialog::rebuildList()
{
    const QString previouslySelected = currentSelectedExe();
    const quint32 previouslySelectedPid = currentSelectedPid();

    m_updatingProgrammatically = true;
    m_list->clear();

    QMap<QString, QList<quint32>> connectedByName;
    for (int i = 0; i < m_connectedGames.size() && i < m_connectedPids.size(); ++i)
        connectedByName[m_connectedGames[i]].append(m_connectedPids[i]);

    QStringList allExe = m_connectedGames + m_searchingGames + m_injectingGames + m_otherKnownGames;
    allExe.removeDuplicates();
    allExe.sort(Qt::CaseInsensitive);

    if (allExe.isEmpty()) {
        auto *placeholder = new QListWidgetItem(tr("<no games connected>"), m_list);
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable);
    } else {
        for (const QString &exe : allExe) {
            QString suffix, tooltip;
            if (!m_connectedGames.contains(exe)) {
                if (m_injectingGames.contains(exe)) {
                    suffix = tr(" (injecting...)");
                    tooltip = tr(
                        "Once the DLL is actually loaded, the connection is only "
                        "made from inside the game's own present call, which some "
                        "games skip entirely while minimized - so this can stay "
                        "here well after injection itself has already succeeded. "
                        "If it doesn't clear, bring the game to the foreground.");
                } else if (m_searchingGames.contains(exe)) {
                    suffix = tr(" (waiting for the game to start...)");
                } else {
                    suffix = tr(" (not running - click to switch)");
                }
            }
            addRows(exe, connectedByName.value(exe), suffix, tooltip);
        }
        selectRow(previouslySelected, previouslySelectedPid);
    }
    m_updatingProgrammatically = false;

#ifdef Q_OS_WIN
    m_removeButton->setEnabled(!currentSelectedExe().isEmpty());
#endif
}

QString OverlayGameSwitcherDialog::currentSelectedExe() const
{
    QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

quint32 OverlayGameSwitcherDialog::currentSelectedPid() const
{
    QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole + 1).toUInt() : 0;
}

void OverlayGameSwitcherDialog::selectRow(const QString &exe, quint32 pid)
{
    auto trySelect = [this](const QString &e, quint32 p) {
        if (e.isEmpty()) return false;
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *it = m_list->item(i);
            if (it->data(Qt::UserRole).toString() == e &&
                (p == 0 || it->data(Qt::UserRole + 1).toUInt() == p)) {
                m_list->setCurrentItem(it);
                return true;
            }
        }
        return false;
    };
    if (trySelect(exe, pid)) return;
    if (trySelect(m_activeExe, m_activePid)) return;
    trySelect(m_managedExe, 0);
}

void OverlayGameSwitcherDialog::addRows(const QString &exe, const QList<quint32> &pids,
                                        const QString &suffix, const QString &tooltip)
{
    if (pids.size() > 1) {
        QList<quint32> sorted = pids;
        std::sort(sorted.begin(), sorted.end());
        for (quint32 pid : sorted) {
            auto *item = new QListWidgetItem(tr("%1 (%2)").arg(exe).arg(pid), m_list);
            item->setData(Qt::UserRole, exe);
            item->setData(Qt::UserRole + 1, pid);
        }
        return;
    }
    auto *item = new QListWidgetItem(exe + suffix, m_list);
    item->setData(Qt::UserRole, exe);
    item->setData(Qt::UserRole + 1, pids.isEmpty() ? 0u : pids.first());
    if (!tooltip.isEmpty()) item->setToolTip(tooltip);
}

#ifdef Q_OS_WIN
void OverlayGameSwitcherDialog::onAddClicked()
{
    OverlayProcessPickerDialog picker(this);
    if (picker.exec() != QDialog::Accepted) return;

    const QString exe = picker.selectedProcess();
    if (exe.isEmpty()) return;

    if (m_searchingGames.contains(exe) || m_injectingGames.contains(exe) ||
        m_otherKnownGames.contains(exe)) {
        QMessageBox::information(this, tr("Add Game"),
            tr("\"%1\" is already in your list of games - select it in the "
               "list to switch to it.").arg(exe));
        return;
    }

    emit gameAddRequested(exe);
}

void OverlayGameSwitcherDialog::onRemoveClicked()
{
    const QString exe = currentSelectedExe();
    if (!exe.isEmpty()) emit gameRemoveRequested(exe);
}
#endif
