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

#include "hooktextmodel.h"
#include <QJsonArray>

// ===============================================================
// Construction & target scoping
// ===============================================================

HookTextModel::HookTextModel(QObject *parent)
    : QObject(parent) {}

void HookTextModel::setTarget(const QString &targetKey)
{
    if (m_currentTarget == targetKey)
        return;

    m_currentTarget = targetKey;
    resyncSavedState();
    emit changed();
}

int HookTextModel::burstMs() const
{
    const int own = m_targets.value(m_currentTarget).burstMs;
    return own == kUnset ? kDefaultBurstMs : own;
}

int HookTextModel::displayMode() const
{
    const int own = m_targets.value(m_currentTarget).displayMode;
    return own == kUnset ? kDefaultDisplayMode : own;
}

void HookTextModel::resyncSavedState()
{
    m_saved = m_targets.value(m_currentTarget).sources;
    m_priority = m_saved;

    rebuildPriorityMembers();

    for (const QString &src : std::as_const(m_sourceOrder)) {
        if (m_saved.contains(src)) {
            m_selected.insert(src);
            m_applied.insert(src);
        }
        if (!m_priorityMembers.contains(src)) {
            m_priority.append(src);
            m_priorityMembers.insert(src);
        }
    }

    rebuildActiveGroups();
}

// ===============================================================
// Runtime feed — register / update / clear seen blocks
// ===============================================================

void HookTextModel::registerSource(const QString &source)
{
    if (m_seen.contains(source))
        return;

    const bool isFirst = m_sourceOrder.isEmpty();

    m_sourceOrder.append(source);
    m_seen.insert(source);

    if (!m_priorityMembers.contains(source)) {
        m_priority.append(source);
        m_priorityMembers.insert(source);
    }

    if (m_memberToGroup.contains(source))
        return;

    if (m_saved.contains(source) || isAutoStream(source) || (isFirst && m_saved.isEmpty())) {
        m_selected.insert(source);
        m_applied.insert(source);
    }
}

void HookTextModel::noteSource(const QString &source, const QString &original)
{
    if (!source.startsWith(QStringLiteral("Hook")))
        return;

    registerSource(source);
    m_latestOriginal.insert(source, original);

    const QString group = m_memberToGroup.value(source);
    if (group.isEmpty()) {
        m_lastSeq.insert(source, ++m_seqCounter);
        m_lastMs.insert(source, QDateTime::currentMSecsSinceEpoch());
    } else {
        registerSource(group);
        m_latestOriginal.insert(group, combinedOriginal(group));
        m_lastSeq.insert(group, ++m_seqCounter);
        m_lastMs.insert(group, QDateTime::currentMSecsSinceEpoch());
    }
    emit previewsChanged();
}

void HookTextModel::ensureRegistered(const QString &source, const QString &original)
{
    if (!source.startsWith(QStringLiteral("Hook")))
        return;

    const bool known = m_sourceOrder.contains(source);
    registerSource(source);
    m_latestOriginal.insert(source, original);

    if (!known) emit previewsChanged();
}

void HookTextModel::clear()
{
    m_sourceOrder.clear();
    m_seen.clear();
    m_selected.clear();
    m_applied.clear();
    m_latestOriginal.clear();
    m_lastSeq.clear();
    m_lastMs.clear();
    m_translatedSeq.clear();
    m_seqCounter = 0;

    resyncSavedState();

    emit changed();
}

// ===============================================================
// Output & display queries (overlay + translation gate)
// ===============================================================

bool HookTextModel::isDisplayed(const QString &source) const
{
    if (!source.startsWith(QStringLiteral("Hook")))
        return true;

    if (m_memberToGroup.contains(source))
        return false;

    if (displayMode() == ShowAllSelected) {
        return m_applied.contains(source);
    }

    return source == activeSource();
}

QStringList HookTextModel::sourcesToOutput(const QStringList &pending) const
{
    QStringList watched;
    for (const QString &raw : pending) {
        const QString src = effectiveSource(raw);

        if (watched.contains(src))
            continue;

        if (!src.startsWith(QStringLiteral("Hook")) || isWatched(src))
            watched << src;
    }

    if (watched.isEmpty()) return {};

    const QHash<QString, int> rank = priorityRank();

    if (displayMode() == ShowAllSelected) {
        std::stable_sort(watched.begin(), watched.end(),
                         [&rank](const QString &a, const QString &b) {
                             return rank.value(a, INT_MAX) < rank.value(b, INT_MAX);
                         });
        return watched;
    }

    qint64 tMax = 0;
    for (const QString &src : std::as_const(watched))
        tMax = qMax(tMax, m_lastMs.value(src, 0));

    const qint64 window = coactiveWindowMs();

    QString best;
    int bestPrio = INT_MAX;
    quint64 bestSeq = 0;
    for (const QString &src : std::as_const(watched)) {
        if (tMax > 0 && m_lastMs.value(src, 0) < tMax - window)
            continue;

        const int prio = rank.value(src, INT_MAX);
        const quint64 seq = m_lastSeq.value(src, 0);
        if (best.isEmpty() || prio < bestPrio || (prio == bestPrio && seq >= bestSeq)) {
            best = src;
            bestPrio = prio;
            bestSeq = seq;
        }
    }
    return best.isEmpty() ? QStringList{} : QStringList{best};
}

QString HookTextModel::activeSource() const
{
    qint64 tMax = 0;
    for (const QString &src : m_applied)
        tMax = qMax(tMax, m_lastMs.value(src, 0));

    QString active;
    if (tMax > 0) {
        int bestPrio = INT_MAX;
        quint64 bestSeq = 0;
        const qint64 window = coactiveWindowMs();
        const QHash<QString, int> rank = priorityRank();

        for (const QString &src : m_applied) {
            if (m_lastMs.value(src, 0) < tMax - window)
                continue;

            const int prio = rank.value(src, INT_MAX);
            const quint64 seq = m_lastSeq.value(src, 0);

            if (prio < bestPrio || (prio == bestPrio && seq >= bestSeq)) {
                bestPrio = prio;
                bestSeq = seq;
                active = src;
            }
        }
    }
    return active;
}

void HookTextModel::markTranslated(const QString &source)
{
    if (!source.startsWith(QStringLiteral("Hook")))
        return;

    m_translatedSeq.insert(source, m_lastSeq.value(source, 0));
}

bool HookTextModel::isStale(const QString &source) const
{
    if (!source.startsWith(QStringLiteral("Hook")))
        return false;

    return m_lastSeq.value(source, 0) > m_translatedSeq.value(source, 0);
}

QHash<QString, int> HookTextModel::priorityRank() const
{
    QHash<QString, int> rank;
    rank.reserve(m_priority.size());
    for (int i = 0; i < m_priority.size(); ++i)
        rank.insert(m_priority.at(i), i);

    return rank;
}

QStringList HookTextModel::displayOrder() const
{
    QStringList ordered;
    QSet<QString> added;

    for (const QString &s : m_priority) {
        if (m_seen.contains(s) && !m_memberToGroup.contains(s)) {
            ordered << s;
            added.insert(s);
        }
    }

    for (const QString &s : m_sourceOrder) {
        if (!added.contains(s) && !m_memberToGroup.contains(s)) {
            ordered << s;
        }
    }

    return ordered;
}

QStringList HookTextModel::savableSelected() const
{
    QStringList out;

    for (const QString &src : m_selected)
        if (src.startsWith(QStringLiteral("Hook:")))
            out << src;

    return out;
}


// ===============================================================
// Source classification helpers (static: auto-stream, variant, label)
// ===============================================================

bool HookTextModel::isAutoStream(const QString &source)
{
    return source == QLatin1String("Hook");
}

int HookTextModel::variantOf(const QString &source)
{
    for (int i = source.size() - 3; i >= 0; --i) {
        if (source.at(i) == QLatin1Char(':') && source.at(i + 1) == QLatin1Char('v')
            && source.at(i + 2).isDigit()) {
            int n = 0;
            for (int j = i + 2; j < source.size() && source.at(j).isDigit(); ++j)
                n = n * 10 + source.at(j).digitValue();
            return n;
        }
    }
    return 0;
}

QString HookTextModel::displayLabel(const QString &source)
{
    for (int i = source.size() - 3; i >= 0; --i) {
        if (source.at(i) == QLatin1Char(':') && source.at(i + 1) == QLatin1Char('v')
            && source.at(i + 2).isDigit()) {
            int j = i + 2;
            while (j < source.size() && source.at(j).isDigit()) ++j;
            return source.left(i) + source.mid(j);
        }
    }
    return source;
}

// ===============================================================
// Grouping — combine several blocks into one translated stream
// ===============================================================

QString HookTextModel::effectiveSource(const QString &source) const
{
    const QString g = m_memberToGroup.value(source);
    return g.isEmpty() ? source : g;
}

QString HookTextModel::combinedOriginal(const QString &groupId) const
{
    QStringList parts;
    const QStringList members = m_groupMembers.value(groupId);
    for (const QString &m : members) {
        const QString t = m_latestOriginal.value(m);
        if (!t.isEmpty())
            parts << t;
    }
    return parts.join(QString());
}

void HookTextModel::groupSources(const QStringList &members)
{
    QStringList clean;
    QStringList absorbed;
    QStringList dropFromOverlay;
    const QStringList order = displayOrder();
    for (const QString &s : order) {
        if (!members.contains(s))
            continue;

        if (m_groupMembers.contains(s)) {
            absorbed << s;
            if (m_applied.contains(s))
                dropFromOverlay << s;
            const QStringList absorbedMembers = m_groupMembers.value(s);
            for (const QString &m : absorbedMembers)
                if (!clean.contains(m))
                    clean << m;
        } else if (!m_memberToGroup.contains(s) && !clean.contains(s)) {
            clean << s;
        }
    }
    if (clean.size() < 2)
        return;

    for (const QString &g : std::as_const(absorbed))
        removeGroup(g);

    const QString gid = makeGroupId();
    m_groupMembers.insert(gid, clean);
    for (const QString &m : std::as_const(clean))
        m_memberToGroup.insert(m, gid);

    int best = INT_MAX;
    for (const QString &m : std::as_const(clean)) {
        const int idx = m_priority.indexOf(m);
        if (idx >= 0)
            best = qMin(best, idx);
    }
    if (best == INT_MAX)
        best = m_priority.size();

    m_priority.insert(best, gid);
    m_priorityMembers.insert(gid);
    m_selected.insert(gid);

    for (const QString &m : std::as_const(clean))
        m_selected.remove(m);

    if (!m_seen.contains(gid)) {
        m_sourceOrder.append(gid);
        m_seen.insert(gid);
    }
    m_latestOriginal.insert(gid, combinedOriginal(gid));

    quint64 maxSeq = 0;
    qint64 maxMs = 0;
    for (const QString &m : std::as_const(clean)) {
        maxSeq = qMax(maxSeq, m_lastSeq.value(m, 0));
        maxMs = qMax(maxMs, m_lastMs.value(m, 0));
    }
    m_lastSeq.insert(gid, maxSeq);
    m_lastMs.insert(gid, maxMs);

    if (!dropFromOverlay.isEmpty()) emit sourcesUnapplied(dropFromOverlay);

    // A new group is session-only (draft) until the user presses Save; only persist
    // here if absorbing existing groups changed the saved set
    if (!absorbed.isEmpty()) emit persistRequested();

    emit changed();
}

void HookTextModel::removeGroup(const QString &groupId)
{
    const QStringList members = m_groupMembers.take(groupId);

    for (const QString &m : members) {
        m_memberToGroup.remove(m);
    }

    m_selected.remove(groupId);
    m_applied.remove(groupId);
    m_priority.removeAll(groupId);
    m_priorityMembers.remove(groupId);
    m_sourceOrder.removeAll(groupId);
    m_seen.remove(groupId);
    m_latestOriginal.remove(groupId);
    m_lastSeq.remove(groupId);
    m_lastMs.remove(groupId);
    m_translatedSeq.remove(groupId);

    QList<QStringList> &groups = m_targets[m_currentTarget].groups;

    for (int i = 0; i < groups.size(); ++i)
        if (groups.at(i) == members) {
            groups.removeAt(i);
            break;
        }
}

void HookTextModel::ungroup(const QString &groupId)
{
    if (!m_groupMembers.contains(groupId))
        return;
    const QStringList members = m_groupMembers.value(groupId);
    removeGroup(groupId);

    for (const QString &m : members)
        if (m_seen.contains(m))
            m_selected.insert(m);

    emit sourcesUnapplied({ groupId });
    emit persistRequested();
    emit changed();
    emit outputReapplyRequested();
}

void HookTextModel::rebuildActiveGroups()
{
    const auto isGroupId = [](const QString &s) {
        if (!s.startsWith(QStringLiteral("Hook:g")) || s.size() == 6)
            return false;
        for (int i = 6; i < s.size(); ++i)   // "Hook:g<digits>"
            if (!s.at(i).isDigit())          // source like "Hook:gxNote" is not a group
                return false;
        return true;
    };

    const auto purgeSet = [&isGroupId](QSet<QString> &s) {
        for (auto it = s.begin(); it != s.end(); )
            it = isGroupId(*it) ? s.erase(it) : std::next(it);
    };

    const auto purgeList = [&isGroupId](QStringList &l) {
        l.erase(std::remove_if(l.begin(), l.end(), isGroupId), l.end());
    };

    purgeSet(m_selected);
    purgeSet(m_applied);
    purgeSet(m_seen);
    purgeSet(m_priorityMembers);
    purgeList(m_sourceOrder);
    purgeList(m_priority);
    for (auto it = m_groupMembers.cbegin(); it != m_groupMembers.cend(); ++it) {
        m_latestOriginal.remove(it.key());
        m_lastSeq.remove(it.key());
        m_lastMs.remove(it.key());
        m_translatedSeq.remove(it.key());
    }
    m_groupMembers.clear();
    m_memberToGroup.clear();

    const QList<QStringList> savedGroups = m_targets.value(m_currentTarget).groups;
    for (const QStringList &members : savedGroups) {
        if (members.size() < 2)
            continue;

        const QString gid = makeGroupId();
        m_groupMembers.insert(gid, members);

        for (const QString &m : members)
            m_memberToGroup.insert(m, gid);

        m_sourceOrder.append(gid);
        m_seen.insert(gid);
        m_priority.append(gid);
        m_priorityMembers.insert(gid);
        m_latestOriginal.insert(gid, combinedOriginal(gid));

        m_selected.insert(gid);
        m_applied.insert(gid);

        for (const QString &m : members) {
            m_selected.remove(m);
            m_applied.remove(m);
        }
    }
}

// ===============================================================
// Selector-driven mutations (draft, mode, priority, apply, save)
// ===============================================================

void HookTextModel::setDraftSelected(const QString &source, bool on)
{
    if (on)
        m_selected.insert(source);
    else
        m_selected.remove(source);
}

void HookTextModel::setDisplayMode(int mode)
{
    if (displayMode() == mode) return;

    m_targets[m_currentTarget].displayMode = mode;

    emit persistRequested();
    emit changed();
}

void HookTextModel::setBurstMs(int ms)
{
    if (burstMs() == ms) return;

    m_targets[m_currentTarget].burstMs = ms;

    emit persistRequested();
}


void HookTextModel::moveHookPriority(const QString &source, int delta)
{
    QStringList ordered = displayOrder();
    const int i = ordered.indexOf(source);
    const int j = i + delta;

    if (i < 0 || j < 0 || j >= ordered.size())
        return;

    ordered.move(i, j);

    for (const QString &s : std::as_const(m_priority))
        if (!ordered.contains(s))
            ordered << s;

    m_priority = ordered;
    rebuildPriorityMembers();

    const QStringList oldSaved = m_saved;
    QStringList newSaved;

    for (const QString &s : std::as_const(m_priority))
        if (oldSaved.contains(s))
            newSaved << s;

    for (const QString &s : oldSaved)
        if (!newSaved.contains(s))
            newSaved << s;

    if (newSaved != oldSaved) {
        m_saved = newSaved;
        m_targets[m_currentTarget].sources = m_saved;
        emit persistRequested();
    }
    emit staleOutputShouldClear();
    emit changed();
}

void HookTextModel::applySelection()
{
    QSet<QString> removedSet = m_applied;
    removedSet.subtract(m_selected);

    if (!removedSet.isEmpty())
        emit sourcesUnapplied(removedSet.values());

    m_applied = m_selected;

    emit changed();
    emit outputReapplyRequested();
}

void HookTextModel::saveSelection()
{
    const QStringList savable = savableSelected();
    if (savable.isEmpty())
        return;

    bool dirty = false;

    QList<QStringList> groups = m_targets.value(m_currentTarget).groups;
    for (const QString &src : savable) {
        if (!m_groupMembers.contains(src))
            continue;
        const QStringList members = m_groupMembers.value(src);
        if (members.size() >= 2 && !groups.contains(members)) {
            groups.append(members);
            dirty = true;
        }
    }
    if (dirty) m_targets[m_currentTarget].groups = groups;

    QStringList newSaved = m_saved;
    const QStringList order = displayOrder();
    for (const QString &src : order)
        if (savable.contains(src) && !m_groupMembers.contains(src) && !newSaved.contains(src))
            newSaved << src;
    for (const QString &src : savable)
        if (!m_groupMembers.contains(src) && !newSaved.contains(src))
            newSaved << src;
    if (newSaved != m_saved) {
        m_saved = newSaved;
        m_targets[m_currentTarget].sources = m_saved;
        dirty = true;
    }

    if (dirty) {
        emit persistRequested();
        emit changed();
    }
}

void HookTextModel::deleteSaved(const QString &source)
{
    m_saved.removeAll(source);
    m_targets[m_currentTarget].sources = m_saved;
    emit persistRequested();
    emit changed();
}

void HookTextModel::deleteSavedGroup(const QStringList &members)
{
    QString gid;
    for (auto it = m_groupMembers.cbegin(); it != m_groupMembers.cend(); ++it)
        if (it.value() == members) {
            gid = it.key();
            break;
        }

    if (!gid.isEmpty()) {
        removeGroup(gid);
        emit sourcesUnapplied({ gid });
    } else {
        m_targets[m_currentTarget].groups.removeAll(members);
    }

    emit persistRequested();
    emit changed();
    emit outputReapplyRequested();
}

// ===============================================================
// Config
// ===============================================================

static QStringList toStringList(const QJsonArray &arr)
{
    QStringList list;
    for (const QJsonValue &v : arr)
        list << v.toString();
    return list;
}

static QList<QStringList> toGroupList(const QJsonArray &arr)
{
    QList<QStringList> groups;
    for (const QJsonValue &gv : arr) {
        const QStringList members = toStringList(gv.toArray());
        if (members.size() >= 2)
            groups.append(members);
    }
    return groups;
}

void HookTextModel::loadConfig(const QJsonObject &output_window)
{
    m_targets.clear();

    const QJsonObject byTarget = output_window["hook_targets"].toObject();
    for (auto it = byTarget.begin(); it != byTarget.end(); ++it) {
        const QJsonObject obj = it.value().toObject();
        TargetState state;
        state.sources = toStringList(obj["sources"].toArray());
        state.groups = toGroupList(obj["groups"].toArray());
        state.burstMs = obj["burst_ms"].toInt(kUnset);
        state.displayMode = obj["display_mode"].toInt(kUnset);
        m_targets.insert(it.key(), state);
    }

    resyncSavedState();

    emit changed();
}

void HookTextModel::saveConfig(QJsonObject &output_window) const
{
    QJsonObject byTarget;
    for (auto it = m_targets.cbegin(); it != m_targets.cend(); ++it) {
        const TargetState &state = it.value();
        QJsonObject obj;

        if (!state.sources.isEmpty()) {
            QJsonArray arr;
            for (const QString &src : state.sources)
                arr.append(src);
            obj["sources"] = arr;
        }

        if (!state.groups.isEmpty()) {
            QJsonArray arr;
            for (const QStringList &members : state.groups) {
                QJsonArray marr;
                for (const QString &m : members)
                    marr.append(m);
                arr.append(marr);
            }
            obj["groups"] = arr;
        }

        if (state.burstMs != kUnset)
            obj["burst_ms"] = state.burstMs;
        if (state.displayMode != kUnset)
            obj["display_mode"] = state.displayMode;

        // A target with nothing saved and nothing changed is not worth storing
        if (!obj.isEmpty())
            byTarget.insert(it.key(), obj);
    }

    output_window["hook_targets"] = byTarget;
}
