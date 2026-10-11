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

#ifndef PLUGINSETTINGSDIALOG_H
#define PLUGINSETTINGSDIALOG_H

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

class QCheckBox;
class QPlainTextEdit;

class PluginSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    PluginSettingsDialog(const QString &title, const QJsonArray &schema, const QJsonObject &values, QWidget *parent = nullptr);

    QJsonObject values() const;
    void addListEntries(const QString &key, const QStringList &entries);

private:
    void fill(const QJsonObject &values);

    QJsonArray m_schema;
    QMap<QString, QCheckBox *> m_checks;
    QMap<QString, QPlainTextEdit *> m_lists;
};

#endif // PLUGINSETTINGSDIALOG_H
