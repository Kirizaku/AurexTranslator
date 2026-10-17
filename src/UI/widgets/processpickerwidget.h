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

#ifndef PROCESSPICKERWIDGET_H
#define PROCESSPICKERWIDGET_H

#include <QWidget>

class QLineEdit;
class QTreeView;
class QPushButton;
class QStandardItemModel;
class QSortFilterProxyModel;

class ProcessPickerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProcessPickerWidget(QWidget *parent = nullptr);

    void refresh();
    QString selectedProcessName() const;

signals:
    void selectionChanged();
    void activated();

private slots:
    void onFilterTextChanged(const QString &text);

private:
    QLineEdit *m_filterEdit = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QTreeView *m_view = nullptr;
    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
};

#endif // PROCESSPICKERWIDGET_H
