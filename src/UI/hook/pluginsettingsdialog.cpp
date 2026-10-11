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

#include "pluginsettingsdialog.h"

#include "src/utils/config.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

namespace {

QString localized(const QJsonValue &value, const QString &fallback = QString())
{
    if (value.isString())
        return value.toString();
    
    if (!value.isObject())
        return fallback;

    const QJsonObject texts = value.toObject();
    const QString language = Config::getValue("general").toJsonObject().value(QStringLiteral("language")).toString();
    for (const QString &key : { language, language.section(QLatin1Char('_'), 0, 0), QStringLiteral("en") }) {
        if (!key.isEmpty() && texts.contains(key))
            return texts.value(key).toString();
    }
    return texts.isEmpty() ? fallback : texts.begin().value().toString();
}

} // namespace

PluginSettingsDialog::PluginSettingsDialog(const QString &title, const QJsonArray &schema,
                                           const QJsonObject &values, QWidget *parent) 
                                           : QDialog(parent), m_schema(schema)
{
    setWindowTitle(title);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(600, 640);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(tr("The plugin reads these once while it loads, so applying them injects it again."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    for (const QJsonValue &value : m_schema) {
        const QJsonObject entry = value.toObject();
        const QString key = entry.value(QStringLiteral("key")).toString();
        const QString type = entry.value(QStringLiteral("type")).toString();
        const QString label = localized(entry.value(QStringLiteral("label")), key);
        const QString hint = localized(entry.value(QStringLiteral("hint")));
        const QString example = localized(entry.value(QStringLiteral("example")));

        if (key.isEmpty())
            continue;

        if (type == QLatin1String("bool")) {
            auto *check = new QCheckBox(label, this);
            m_checks.insert(key, check);
            layout->addWidget(check);
        } else if (type == QLatin1String("list")) {
            layout->addWidget(new QLabel(label, this));
            if (!hint.isEmpty()) {
                auto *note = new QLabel(hint, this);
                note->setWordWrap(true);
                note->setEnabled(false);
                layout->addWidget(note);
            }
            auto *edit = new QPlainTextEdit(this);
            edit->setMinimumHeight(150);
            edit->setPlaceholderText(example.isEmpty() ? hint : example);
            m_lists.insert(key, edit);
            layout->addWidget(edit, 1);
            continue;
        } else {
            continue;
        }

        if (!hint.isEmpty()) {
            auto *note = new QLabel(hint, this);
            note->setWordWrap(true);
            note->setContentsMargins(24, 0, 0, 6);
            note->setEnabled(false);
            layout->addWidget(note);
        }
    }
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::RestoreDefaults, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Restore defaults"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] { fill(QJsonObject()); });
    layout->addWidget(buttons);

    fill(values);
}

void PluginSettingsDialog::fill(const QJsonObject &values)
{
    for (const QJsonValue &value : m_schema) {
        const QJsonObject entry = value.toObject();
        const QString key = entry.value(QStringLiteral("key")).toString();
        const QJsonValue current = values.contains(key) ? values.value(key) : entry.value(QStringLiteral("default"));

        if (QCheckBox *check = m_checks.value(key, nullptr)) {
            check->setChecked(current.toBool(false));
        } else if (QPlainTextEdit *edit = m_lists.value(key, nullptr)) {
            QStringList lines;
            for (const QJsonValue &item : current.toArray())
                lines << item.toString();
            edit->setPlainText(lines.join(QLatin1Char('\n')));
        }
    }
}

QJsonObject PluginSettingsDialog::values() const
{
    QJsonObject result;
    for (auto it = m_checks.constBegin(); it != m_checks.constEnd(); ++it)
        result[it.key()] = it.value()->isChecked();

    for (auto it = m_lists.constBegin(); it != m_lists.constEnd(); ++it) {
        QJsonArray lines;
        for (const QString &line : it.value()->toPlainText().split(QLatin1Char('\n'))) {
            if (!line.trimmed().isEmpty())
                lines.append(line.trimmed());
        }
        result[it.key()] = lines;
    }
    return result;
}

void PluginSettingsDialog::addListEntries(const QString &key, const QStringList &entries)
{
    QPlainTextEdit *edit = m_lists.value(key, nullptr);
    if (!edit)
        return;

    QStringList lines = edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        if (!lines.contains(entry))
            lines << entry;
    }
    edit->setPlainText(lines.join(QLatin1Char('\n')));
    edit->moveCursor(QTextCursor::End);
}
