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

#ifndef CLIPBOARDCONTROLLER_H
#define CLIPBOARDCONTROLLER_H

#include <QObject>
#include <QString>

class ClipboardController : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardController(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ClipboardController() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    void suppress(const QString &text) { m_suppressedText = text; }

    static ClipboardController *create(QObject *parent = nullptr);

signals:
    void textChanged(const QString &text);
    void failed();

protected:
    bool checkAndClearSuppressed(const QString &text)
    {
        if (!m_suppressedText.isEmpty() && text == m_suppressedText) {
            m_suppressedText.clear();
            return true;
        }
        return false;
    }

    QString m_suppressedText;
};

#endif // CLIPBOARDCONTROLLER_H
