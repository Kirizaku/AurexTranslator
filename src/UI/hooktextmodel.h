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

#ifndef HOOKTEXTMODEL_H
#define HOOKTEXTMODEL_H

#include <QObject>
#include <QJsonObject>

class HookTextModel : public QObject
{
    Q_OBJECT

public:
    enum DisplayMode {
        FollowLastChanged = 0,  // show only the applied block that changed last
        ShowAllSelected   = 1   // show every applied block at once
    };

    explicit HookTextModel(QObject *parent = nullptr);
    void setTarget(const QString &targetKey);
    void noteSource(const QString &source, const QString &original);
    void ensureRegistered(const QString &source, const QString &original);
    void clear();

    bool isDisplayed(const QString &source) const;
    QStringList sourcesToOutput(const QStringList &pending) const;

    int burstMs() const;

    void markTranslated(const QString &source);
    bool isStale(const QString &source) const;

    int displayMode() const;
    QStringList displayOrder() const;
    bool isDraftSelected(const QString &source) const { return m_selected.contains(source); }
    QStringList savedSources() const { return m_saved; }
    QList<QStringList> savedGroups() const { return m_targets.value(m_currentTarget).groups; }
    QString latestOriginal(const QString &source) const { return m_latestOriginal.value(source); }
    QStringList savableSelected() const;

    // Variant
    static int variantOf(const QString &source);
    static bool isAutoStream(const QString &source);
    static QString displayLabel(const QString &source);

    // Manual grouping
    void groupSources(const QStringList &members);
    void ungroup(const QString &groupId);
    bool isGroup(const QString &source) const { return m_groupMembers.contains(source); }
    QString effectiveSource(const QString &source) const;
    QStringList groupMembers(const QString &groupId) const { return m_groupMembers.value(groupId); }
    QString combinedOriginal(const QString &groupId) const;

    void setDraftSelected(const QString &source, bool on);
    void setDisplayMode(int mode);
    void setBurstMs(int ms);
    void moveHookPriority(const QString &source, int delta);
    void applySelection();
    void saveSelection();
    void deleteSaved(const QString &source);
    void deleteSavedGroup(const QStringList &members);
    void requestClear() { emit clearRequested(); }
    void syncDraftToApplied() { m_selected = m_applied; }

    // Config
    void loadConfig(const QJsonObject &output_window);
    void saveConfig(QJsonObject &output_window) const;

signals:
    void changed();
    void persistRequested();
    void previewsChanged();
    void sourcesUnapplied(const QStringList &sources);
    void outputReapplyRequested();
    void clearRequested();
    void staleOutputShouldClear();

private:
    void registerSource(const QString &source);
    void resyncSavedState();
    bool isWatched(const QString &source) const { return m_applied.contains(source); }
    QString activeSource() const;
    QHash<QString, int> priorityRank() const;
    void rebuildPriorityMembers() { m_priorityMembers = QSet<QString>(m_priority.cbegin(), m_priority.cend()); }

    QStringList m_sourceOrder;
    QSet<QString> m_seen;
    QSet<QString> m_priorityMembers;
    QSet<QString> m_selected;
    QSet<QString> m_applied;
    QHash<QString, QString> m_latestOriginal;
    QHash<QString, quint64> m_lastSeq;
    QHash<QString, qint64> m_lastMs;
    QHash<QString, quint64> m_translatedSeq;
    quint64 m_seqCounter = 0;

    static constexpr int kUnset = -1;
    static constexpr int kDefaultDisplayMode = FollowLastChanged;
    static constexpr int kDefaultBurstMs = 150;

    struct TargetState {
        QStringList sources;          // saved addresses, in priority order
        QList<QStringList> groups;    // saved manual groups
        int burstMs = kUnset;
        int displayMode = kUnset;
    };

    QHash<QString, TargetState> m_targets;

    // Priority order
    static constexpr qint64 kCoactiveWindowMs = 400;
    qint64 coactiveWindowMs() const { return qMax<qint64>(burstMs(), kCoactiveWindowMs); }
    QStringList m_priority;

    // Saved addresses
    QString m_currentTarget;
    QStringList m_saved;

    // Manual groups
    QHash<QString, QStringList> m_groupMembers;   // groupId -> ordered members
    QHash<QString, QString> m_memberToGroup;      // member source -> groupId
    void rebuildActiveGroups();
    void removeGroup(const QString &groupId);
    QString makeGroupId() const {
        for (int n = 1; ; ++n) {
            const QString gid = QStringLiteral("Hook:g%1").arg(n);
            if (!m_groupMembers.contains(gid))
                return gid;
        }
    }
};

#endif // HOOKTEXTMODEL_H
