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

#ifndef OVERLAYGAMESWITCHERDIALOG_H
#define OVERLAYGAMESWITCHERDIALOG_H

#include <QDialog>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QStringList>

class QResizeEvent;
class QShowEvent;
class QHideEvent;
class QPushButton;

class OverlayGameSwitcherDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OverlayGameSwitcherDialog(QWidget *parent = nullptr);

public slots:
    void setGameList(const QStringList &exeNames, const QList<quint32> &pids);

#ifdef Q_OS_WIN
    void setManagedGames(const QStringList &searchingGames, const QStringList &injectingGames);
    void setKnownGames(const QStringList &otherGames);
#endif
    void setActiveGame(const QString &exe, quint32 pid);
    void setPreviewFrame(const QImage &frame);
    void clearPreview();
    void setPreviewMessage(const QString &message);

signals:
    void gameSelected(const QString &exe, quint32 pid);
    void launchHelpRequested();
#ifdef Q_OS_WIN
    void gameAddRequested(const QString &exe);
    void gameRemoveRequested(const QString &exe);
    void gameSwitchRequested(const QString &exe);
#endif
    void switcherShown();
    void switcherHidden();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void rescalePreview();
    void rebuildList();
    QString currentSelectedExe() const;
    quint32 currentSelectedPid() const;
    void selectRow(const QString &exe, quint32 pid);
    void addRows(const QString &exe, const QList<quint32> &pids, const QString &suffix = QString(), const QString &tooltip = QString());
#ifdef Q_OS_WIN
    void onAddClicked();
    void onRemoveClicked();
#endif

    QListWidget *m_list;
    QLabel *m_preview;
    QImage m_lastFrame;
    bool m_updatingProgrammatically = false;

    QStringList m_connectedGames;
    QList<quint32> m_connectedPids;

    QString m_activeExe;
    quint32 m_activePid = 0;

    QStringList m_searchingGames;
    QStringList m_injectingGames;
    QStringList m_otherKnownGames;
    QString m_managedExe;
#ifdef Q_OS_WIN
    QPushButton *m_removeButton = nullptr;
#endif
};

#endif // OVERLAYGAMESWITCHERDIALOG_H
