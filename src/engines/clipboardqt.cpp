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

#include "clipboardqt.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

QtClipboardController::QtClipboardController(QObject *parent)
    : ClipboardController(parent) {}

void QtClipboardController::start()
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &QtClipboardController::onDataChanged);
}

void QtClipboardController::stop()
{
    disconnect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &QtClipboardController::onDataChanged);
}

void QtClipboardController::onDataChanged()
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime || !mime->hasText())
        return;

    const QString text = mime->text();
    if (!text.isEmpty() && !checkAndClearSuppressed(text))
        emit textChanged(text);
}
