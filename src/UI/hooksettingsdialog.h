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

#ifndef HOOKSETTINGSDIALOG_H
#define HOOKSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QTreeView>
#include <QStackedWidget>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

class HookSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    enum HookMode {
        GameAppMode,
        EngineMode
    };

    explicit HookSettingsDialog(HookMode currentMode,
                                const QMap<QString, QString> &gameAppList,
                                const QMap<QString, QString> &engineList,
                                QString &currentGameAppPlugin,
                                QString &currentEnginePlugin,
                                QWidget *parent = nullptr);
    ~HookSettingsDialog();

    HookMode getCurrentMode() const;

    // GameAppMode
    QString getCurrentGameAppPlugin() const;

    // EngineMode
    QString getSelectedEngine() const;
    QString getSelectedProcessName() const;

    void setEngineList(const QStringList &engines, const QString &current = QString());

private slots:
    void onModeChanged();
    void onRefreshProcesses();
    void onFilterTextChanged(const QString &text);
    void onEngineChanged(int index);

private:
    void loadProcesses();

    QRadioButton *m_gameAppRadio = nullptr;
    QRadioButton *m_engineRadio = nullptr;

    QStackedWidget *m_stack = nullptr;

    // Application
    QComboBox *m_gameListComboBox = nullptr;

    // Engine
    QComboBox *m_engineComboBox = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTreeView *m_processView = nullptr;
    QStandardItemModel *m_processModel = nullptr;
    QSortFilterProxyModel *m_processProxy = nullptr;
    QPushButton *m_refreshButton = nullptr;
};

#endif // HOOKSETTINGSDIALOG_H