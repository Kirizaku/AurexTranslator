/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

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

#include "hookselectordialog.h"
#include "hooktextmodel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QMessageBox>
#include <QTimer>
#include <QMouseEvent>

namespace {

class ClickCard : public QFrame
{
public:
    explicit ClickCard(QWidget *parent = nullptr) : QFrame(parent)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
    }
    std::function<void()> onClick;

protected:

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            e->accept();
            return;
        }
        QFrame::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && onClick)
            onClick();

        QFrame::mouseReleaseEvent(e);
    }
};

QString rgba(const QColor &c, int a)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
}

void setCardPicked(QFrame *card, bool on)
{
    card->setProperty("selected", on);
    card->style()->unpolish(card);
    card->style()->polish(card);
}

} // namespace


static QString sourceLabel(const QString &source)
{
    QString label = HookTextModel::displayLabel(source);
    const int v = HookTextModel::variantOf(source);
    if (v > 0)
        label += HookSelectorDialog::tr("  [variant %1]").arg(v);
    return label;
}

static void clearLayout(QLayout *layout)
{
    if (!layout)
        return;
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

HookSelectorDialog::HookSelectorDialog(HookTextModel *model, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Hook text selection"));
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    resize(440, 420);

    const QPalette pal = palette();
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor muted = pal.color(QPalette::Text);

    setStyleSheet(QStringLiteral(
        "QFrame#hookCard{border:1px solid palette(mid);border-radius:6px;background:transparent;}"
        "QFrame#hookCard:hover{background:%1;}"
        "QFrame#hookCard[selected=\"true\"]{border:1px solid %2;background:%3;}"
        "QFrame#hookCard[selected=\"true\"]:hover{background:%4;}"
        "QLabel#hookAddr{font-weight:600;}"
        "QLabel#hookMeta{color:%5;}"
        "QLabel#hookBadge{background:%6;border-radius:6px;padding:0px 6px;color:%2;font-weight:600;}"
        "QFrame#actionBar{border-top:1px solid palette(mid);}"
        "QToolButton#chipBtn{border:none;background:transparent;padding:2px 5px;border-radius:4px;}"
        "QToolButton#chipBtn:hover{background:%1;}"
        "QFrame#groupChip{border:1px solid %2;border-radius:6px;background:%3;}"
        "QLabel#groupChipLabel{font-weight:600;padding-left:6px;}")
        .arg(rgba(accent, 26), accent.name(), rgba(accent, 42),
             rgba(accent, 60), rgba(muted, 150), rgba(accent, 55)));

    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(8);

    auto *hint = new QLabel(
        tr("Some games show several text blocks at once (for example a choice "
           "menu). Tick a block's checkbox to show it in the overlay. Click a row "
           "to pick it for grouping (highlighted), then press Group to translate "
           "the picked blocks as one."),
        this);

    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("hookMeta"));
    outer->addWidget(hint);

    auto *search = new QLineEdit(this);
    search->setPlaceholderText(tr("Search blocks by address or text…"));
    search->setClearButtonEnabled(true);
    connect(search, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_filter = t.trimmed();
        refresh();
    });
    outer->addWidget(search);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Show only the block that changed last"), HookTextModel::FollowLastChanged);
    m_modeCombo->addItem(tr("Show all ticked blocks together"), HookTextModel::ShowAllSelected);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_model->setDisplayMode(m_modeCombo->currentData().toInt());
    });
    outer->addWidget(m_modeCombo);

    m_waitWidget = new QWidget(this);
    auto *waitRow = new QHBoxLayout(m_waitWidget);
    waitRow->setContentsMargins(0, 0, 0, 0);
    auto *waitLabel = new QLabel(tr("Wait for other blocks:"), m_waitWidget);
    m_waitSpin = new QSpinBox(m_waitWidget);
    m_waitSpin->setRange(0, 2000);
    m_waitSpin->setSingleStep(50);
    m_waitSpin->setSuffix(tr(" ms"));

    const QString waitTip = tr("When two blocks change at almost the same time, wait this long after the "
                               "first one before deciding which to show, then show the higher-priority "
                               "block (set the order with the ▲ ▼ arrows). Increase it if a lower "
                               "block still flashes before your preferred one; 0 shows whatever changes first.");

    waitLabel->setToolTip(waitTip);
    m_waitSpin->setToolTip(waitTip);
    connect(m_waitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) { m_model->setBurstMs(v); });
    waitRow->addWidget(waitLabel, 1);
    waitRow->addWidget(m_waitSpin);
    outer->addWidget(m_waitWidget);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    // Top pane: live sources list.
    auto *livePane = new QWidget(splitter);
    auto *liveLayout = new QVBoxLayout(livePane);
    liveLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(livePane);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *container = new QWidget(scroll);
    m_liveList = new QVBoxLayout(container);
    m_liveList->setSpacing(6);
    m_liveList->setAlignment(Qt::AlignTop);
    scroll->setWidget(container);
    liveLayout->addWidget(scroll, 1);

    // Bottom pane: saved addresses
    auto *savedPane = new QWidget(splitter);
    auto *savedLayout = new QVBoxLayout(savedPane);
    savedLayout->setContentsMargins(0, 0, 0, 0);

    auto *savedHint = new QLabel(
        tr("Saved addresses (auto-applied when they reappear):"), savedPane);

    savedHint->setWordWrap(true);
    savedHint->setObjectName(QStringLiteral("hookMeta"));
    savedLayout->addWidget(savedHint);

    auto *savedScroll = new QScrollArea(savedPane);
    savedScroll->setWidgetResizable(true);
    savedScroll->setFrameShape(QFrame::NoFrame);
    auto *savedContainer = new QWidget(savedScroll);
    m_savedList = new QVBoxLayout(savedContainer);
    m_savedList->setSpacing(4);
    m_savedList->setAlignment(Qt::AlignTop);
    savedScroll->setWidget(savedContainer);
    savedLayout->addWidget(savedScroll, 1);

    splitter->addWidget(livePane);
    splitter->addWidget(savedPane);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 260, 120 });
    outer->addWidget(splitter, 1);

    // Group / Apply / Save / Clear
    auto *bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("actionBar"));
    auto *barRow = new QHBoxLayout(bar);
    barRow->setContentsMargins(0, 6, 0, 0);

    m_groupBtn = new QPushButton(tr("Group"), bar);
    m_groupBtn->setToolTip(tr(
        "Combine the selected blocks into one: their texts are joined and translated "
        "together, shown as a single block. Select at least two first."));

    connect(m_groupBtn, &QPushButton::clicked, this, [this] {
        QStringList picked;
        const QStringList order = m_model->displayOrder();
        for (const QString &src : order)
            if (m_groupPick.contains(src))
                picked << src;
        if (picked.size() < 2) {
            QMessageBox::information(this, tr("Group blocks"),
                tr("Click at least two blocks or groups to highlight and combine them."));
            return;
        }
        m_groupPick.clear();
        m_model->groupSources(picked);
    });
    barRow->addWidget(m_groupBtn);

    barRow->addStretch(1);

    auto *applyBtn = new QPushButton(tr("Apply"), bar);
    applyBtn->setToolTip(tr(
        "Show the current text for the selected blocks now. Without this a block you "
        "just selected appears only on its next update."));
    connect(applyBtn, &QPushButton::clicked, this, [this] { m_model->applySelection(); });
    barRow->addWidget(applyBtn);

    auto *saveBtn = new QPushButton(tr("Save"), bar);
    saveBtn->setToolTip(tr("Remember the selected blocks so they re-apply next launch."));
    connect(saveBtn, &QPushButton::clicked, this, [this] {

        const QStringList savable = m_model->savableSelected();
        if (savable.isEmpty()) {
            QMessageBox::information(this, tr("Save selection"),
                tr("Select at least one specific block to save - an address (Hook:0x...), "
                   "a variant (Hook:v1, Hook:v2, ...), a named source (Hook:Textbox, ...) "
                   "or a group. The plain \"Hook\" stream cannot be saved."));
            return;
        }

        bool anyAddress = false;
        for (const QString &src : savable) {
            if (src.contains(QStringLiteral(":0x")))
                anyAddress = true;
            else if (m_model->isGroup(src)) {
                const QStringList members = m_model->groupMembers(src);
                for (const QString &m : members)
                    if (m.contains(QStringLiteral(":0x")))
                        anyAddress = true;
            }
        }

        if (anyAddress && !m_addrSaveWarned) {
            const auto answer = QMessageBox::warning(this, tr("Save selection"),
                tr("Hook addresses are runtime memory pointers. In some games they "
                   "are stable and will be restored on the next launch; in others "
                   "they change every run and won't match.\n\nSave anyway?"),
                QMessageBox::Save | QMessageBox::Cancel);
            if (answer != QMessageBox::Save)
                return;
            m_addrSaveWarned = true;
        }
        m_model->saveSelection();
    });
    barRow->addWidget(saveBtn);

    auto *clearBtn = new QPushButton(tr("Clear"), bar);

    clearBtn->setToolTip(tr("Forget all hook text seen so far and empty the overlay. Saved addresses "
                            "are kept. Useful after a per-character variant flooded the list."));

    connect(clearBtn, &QPushButton::clicked, this, [this] { m_model->requestClear(); });
    barRow->addWidget(clearBtn);

    outer->addWidget(bar);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(120);
    connect(m_refreshTimer, &QTimer::timeout, this, [this] { if (isVisible()) refresh(); });

    connect(m_model, &HookTextModel::changed, this, [this] { if (isVisible()) refresh(); });
    connect(m_model, &HookTextModel::previewsChanged, this, [this] {
        if (isVisible() && !m_refreshTimer->isActive())
            m_refreshTimer->start();
    });
}

void HookSelectorDialog::openFresh()
{
    m_model->syncDraftToApplied();
    refresh();
    show();
    raise();
    activateWindow();
}

void HookSelectorDialog::updateGroupButton()
{
    int n = 0;
    const QStringList order = m_model->displayOrder();
    for (const QString &src : order)
        if (m_groupPick.contains(src))
            ++n;

    m_groupBtn->setText(n > 0 ? tr("Group (%1)").arg(n) : tr("Group"));
    m_groupBtn->setEnabled(n >= 2);
}

void HookSelectorDialog::refreshScopedSettings()
{
    // The display mode and the burst window belong to the current target, so they
    // have to follow it instead of keeping whatever the previous game used
    const QSignalBlocker blockMode(m_modeCombo);
    const int idx = m_modeCombo->findData(m_model->displayMode());
    m_modeCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    const QSignalBlocker blockWait(m_waitSpin);
    m_waitSpin->setValue(m_model->burstMs());
}

void HookSelectorDialog::refresh()
{
    refreshScopedSettings();

    const int mode = m_model->displayMode();

    if (m_waitWidget)
        m_waitWidget->setVisible(mode == HookTextModel::FollowLastChanged);

    if (m_liveList) {
        clearLayout(m_liveList);

        const QStringList ordered = m_model->displayOrder();
        if (ordered.isEmpty()) {
            auto *empty = new QLabel(tr("(no hook text yet)"));
            empty->setObjectName(QStringLiteral("hookMeta"));
            m_liveList->addWidget(empty);
        } else {
            if (ordered.size() > 1) {
                auto *prioHint = new QLabel(tr(
                    "Order blocks with ▲ ▼ - the higher one wins when "
                    "several change at once, and sets the output order when all "
                    "are shown. Only saved addresses keep their order next time."));

                prioHint->setWordWrap(true);
                prioHint->setObjectName(QStringLiteral("hookMeta"));
                m_liveList->addWidget(prioHint);
            }
            for (int i = 0; i < ordered.size(); ++i) {
                const QString src = ordered.at(i);
                const bool isGroup = m_model->isGroup(src);

                QString addrText = HookTextModel::displayLabel(src);
                QString groupTip;
                if (isGroup) {
                    const QStringList membs = m_model->groupMembers(src);
                    addrText = tr("Group (%1)").arg(membs.size());
                    QStringList parts;
                    for (const QString &m : membs)
                        parts << sourceLabel(m);

                    groupTip = parts.join(QStringLiteral("\n"));
                }
                const QString original = m_model->latestOriginal(src);

                if (!m_filter.isEmpty()
                    && !addrText.contains(m_filter, Qt::CaseInsensitive)
                    && !original.contains(m_filter, Qt::CaseInsensitive))
                    continue;

                auto *card = new ClickCard;
                card->setObjectName(QStringLiteral("hookCard"));
                auto *row = new QHBoxLayout(card);
                row->setContentsMargins(8, 6, 8, 6);
                row->setSpacing(8);

                auto *enable = new QCheckBox(card);
                enable->setChecked(m_model->isDraftSelected(src));
                enable->setToolTip(tr("Show this block in the overlay (press Apply to take effect)."));
                connect(enable, &QCheckBox::toggled, this,
                        [this, src](bool on) { m_model->setDraftSelected(src, on); });
                row->addWidget(enable);

                const int variant = HookTextModel::variantOf(src);
                if (isGroup) {
                    auto *chip = new QFrame(card);
                    chip->setObjectName(QStringLiteral("groupChip"));
                    auto *chipRow = new QHBoxLayout(chip);
                    chipRow->setContentsMargins(0, 0, 0, 0);
                    chipRow->setSpacing(0);
                    auto *chipLabel = new QLabel(addrText, chip);
                    chipLabel->setObjectName(QStringLiteral("groupChipLabel"));
                    if (!groupTip.isEmpty())
                        chipLabel->setToolTip(groupTip);
                    chipRow->addWidget(chipLabel);
                    auto *ungroup = new QToolButton(chip);
                    ungroup->setObjectName(QStringLiteral("chipBtn"));
                    ungroup->setText(QStringLiteral("X"));
                    ungroup->setToolTip(tr("Ungroup: split this group back into separate blocks."));
                    connect(ungroup, &QToolButton::clicked, this,
                            [this, src] { m_model->ungroup(src); });
                    chipRow->addWidget(ungroup);
                    row->addWidget(chip);
                } else {
                    auto *addr = new QLabel(addrText, card);
                    addr->setObjectName(QStringLiteral("hookAddr"));
                    row->addWidget(addr);
                    if (variant > 0) {
                        auto *badge = new QLabel(tr("v%1").arg(variant), card);
                        badge->setObjectName(QStringLiteral("hookBadge"));
                        row->addWidget(badge);
                    }
                }

                auto *preview = new QLabel(original, card);
                preview->setObjectName(QStringLiteral("hookMeta"));
                preview->setTextFormat(Qt::PlainText);
                preview->setWordWrap(false);
                preview->setToolTip(original);
                preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                row->addWidget(preview, 1);

                auto *up = new QPushButton(QStringLiteral("▲"), card);
                auto *down = new QPushButton(QStringLiteral("▼"), card);
                up->setFixedWidth(28);
                down->setFixedWidth(28);
                up->setToolTip(tr("Higher priority"));
                down->setToolTip(tr("Lower priority"));
                up->setEnabled(i > 0);
                down->setEnabled(i < ordered.size() - 1);
                connect(up, &QPushButton::clicked, this, [this, src] { m_model->moveHookPriority(src, -1); });
                connect(down, &QPushButton::clicked, this, [this, src] { m_model->moveHookPriority(src, +1); });
                row->addWidget(up);
                row->addWidget(down);

                setCardPicked(card, m_groupPick.contains(src));
                card->onClick = [this, src, card] {
                    if (m_groupPick.contains(src))
                        m_groupPick.remove(src);
                    else
                        m_groupPick.insert(src);
                    setCardPicked(card, m_groupPick.contains(src));
                    updateGroupButton();
                };

                m_liveList->addWidget(card);
            }
        }
    }

    // Saved addresses list
    if (m_savedList) {
        clearLayout(m_savedList);

        const QStringList saved = m_model->savedSources();
        const QList<QStringList> savedGroups = m_model->savedGroups();
        if (saved.isEmpty() && savedGroups.isEmpty()) {
            auto *empty = new QLabel(tr("(nothing saved)"));
            empty->setObjectName(QStringLiteral("hookMeta"));
            m_savedList->addWidget(empty);
        } else {
            for (const QString &src : saved) {
                auto *row = new QWidget;
                auto *h = new QHBoxLayout(row);
                h->setContentsMargins(0, 0, 0, 0);
                h->addWidget(new QLabel(sourceLabel(src)), 1);

                auto *del = new QPushButton(tr("Delete"));
                del->setToolTip(tr("Forget this saved address."));
                connect(del, &QPushButton::clicked, this, [this, src] { m_model->deleteSaved(src); });
                h->addWidget(del);

                m_savedList->addWidget(row);
            }

            for (const QStringList &members : savedGroups) {
                auto *row = new QWidget;
                auto *h = new QHBoxLayout(row);
                h->setContentsMargins(0, 0, 0, 0);
                h->setSpacing(8);

                auto *tag = new QLabel(tr("Group (%1)").arg(members.size()));
                tag->setObjectName(QStringLiteral("hookAddr"));
                h->addWidget(tag);

                QString combined;
                QStringList tip;
                for (const QString &m : members) {
                    combined += m_model->latestOriginal(m);
                    tip << sourceLabel(m);
                }
                if (combined.isEmpty()) {
                    QStringList addrs;
                    for (const QString &m : members)
                        addrs << HookTextModel::displayLabel(m);
                    combined = addrs.join(QStringLiteral(", "));
                }
                auto *desc = new QLabel(combined);
                desc->setObjectName(QStringLiteral("hookMeta"));
                desc->setTextFormat(Qt::PlainText);
                desc->setWordWrap(false);
                desc->setToolTip(tip.join(QStringLiteral("\n")));
                desc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                h->addWidget(desc, 1);

                auto *del = new QPushButton(tr("Delete"));
                del->setToolTip(tr("Forget this saved group."));
                connect(del, &QPushButton::clicked, this,
                        [this, members] { m_model->deleteSavedGroup(members); });
                h->addWidget(del);

                m_savedList->addWidget(row);
            }
        }
    }

    updateGroupButton();
}
