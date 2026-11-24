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
#include <QSet>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include "overlayprocesspickerdialog.h"
#include <QMessageBox>
#endif

namespace {

QString gameNameKey(const QString &name)
{
#ifdef Q_OS_WIN
    return name.toCaseFolded();
#else
    return name;
#endif
}

bool containsGameName(const QStringList &names, const QString &name)
{
    const QString key = gameNameKey(name);
    for (const QString &candidate : names) {
        if (gameNameKey(candidate) == key)
            return true;
    }
    return false;
}

} // namespace

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

#ifdef Q_OS_WIN
    if (!m_lastSelectedExe.isEmpty() || !m_managedExe.isEmpty()) return;
#endif
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

void OverlayGameSwitcherDialog::setSelectedGame(const QString &exe)
{
    if (m_lastSelectedExe == exe) return;
    m_lastSelectedExe = exe;
    rebuildList();
}

void OverlayGameSwitcherDialog::rebuildList()
{
    const QString previouslySelected = currentSelectedExe();
    const quint32 previouslySelectedPid = currentSelectedPid();

    m_updatingProgrammatically = true;
    m_list->clear();

    QMap<QString, QList<quint32>> connectedByName;
    for (int i = 0; i < m_connectedGames.size() && i < m_connectedPids.size(); ++i) {
        const QString key = gameNameKey(m_connectedGames[i]);
        auto &pids = connectedByName[key];
        const quint32 pid = m_connectedPids[i];
        if (!pids.contains(pid))
            pids.append(pid);
    }

    QStringList allExe;
    QSet<QString> seenNames;
    const QStringList nameSources = m_connectedGames + m_searchingGames + m_injectingGames + m_otherKnownGames;
    for (const QString &exe : nameSources) {
        const QString key = gameNameKey(exe);
        if (key.isEmpty() || seenNames.contains(key))
            continue;

        seenNames.insert(key);
        allExe.append(exe);
    }
    allExe.sort(Qt::CaseInsensitive);

    if (allExe.isEmpty()) {
        auto *placeholder = new QListWidgetItem(tr("<no games connected>"), m_list);
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable);
    } else {
        for (const QString &exe : allExe) {
            QString suffix, tooltip;
            if (!containsGameName(m_connectedGames, exe)) {
                if (containsGameName(m_searchingGames, exe)) {
                    suffix = tr(" (waiting for the game to start...)");
                } else if (!containsGameName(m_injectingGames, exe)) {
                    suffix = tr(" (not running - click to switch)");
                }
            }
            addRows(exe, connectedByName.value(gameNameKey(exe)), suffix, tooltip);
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
            if (gameNameKey(it->data(Qt::UserRole).toString()) == gameNameKey(e) &&
                (p == 0 || it->data(Qt::UserRole + 1).toUInt() == p)) {
                m_list->setCurrentItem(it);
                return true;
            }
        }
        return false;
    };
    if (!m_lastSelectedExe.isEmpty()) {
        if (gameNameKey(exe) == gameNameKey(m_lastSelectedExe) && trySelect(exe, pid)) return;
        if (!trySelect(m_lastSelectedExe, 0)) m_list->setCurrentRow(-1);
        return;
    }
    if (trySelect(exe, pid)) return;
    if (trySelect(m_managedExe, 0)) return;
    trySelect(m_activeExe, m_activePid);
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

    if (containsGameName(m_searchingGames, exe) || containsGameName(m_injectingGames, exe) ||
        containsGameName(m_otherKnownGames, exe)) {
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
    if (exe.isEmpty()) return;
    emit gameRemoveRequested(exe);
}
#endif
