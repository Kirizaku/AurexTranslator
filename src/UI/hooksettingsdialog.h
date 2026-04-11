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

class HookSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HookSettingsDialog(const QMap<QString, QString> &gameList, QString &currentPlugin, QWidget *parent);
    ~HookSettingsDialog();

    QString getCurrentNamePlugin() const;

private:
    QComboBox *gameListComboBox;
};

#endif // HOOKSETTINGSDIALOG_H
