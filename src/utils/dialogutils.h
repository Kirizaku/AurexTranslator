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

#ifndef DIALOGUTILS_H
#define DIALOGUTILS_H

#include <QMessageBox>
#include <QInputDialog>

// The TextOutputWindow overlay is Qt::WindowStaysOnTopHint, so a plain modal
// dialog opened while it is visible gets hidden behind it. These helpers mirror
// the QMessageBox/QInputDialog convenience functions but tag the dialog with the
// same stays-on-top hint, keeping it in the top layer above the overlay

namespace DialogUtils {

inline QMessageBox::StandardButton showMessage(QWidget *parent,
                                               QMessageBox::Icon icon,
                                               const QString &title,
                                               const QString &text,
                                               QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                               QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    QMessageBox box(icon, title, text, buttons, parent);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    if (defaultButton != QMessageBox::NoButton)
        box.setDefaultButton(defaultButton);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

inline QMessageBox::StandardButton warning(QWidget *parent, const QString &title, const QString &text)
{
    return showMessage(parent, QMessageBox::Warning, title, text);
}

inline QMessageBox::StandardButton information(QWidget *parent, const QString &title, const QString &text)
{
    return showMessage(parent, QMessageBox::Information, title, text);
}

inline QMessageBox::StandardButton question(QWidget *parent,
                                            const QString &title,
                                            const QString &text,
                                            QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No)
{
    return showMessage(parent, QMessageBox::Question, title, text, buttons);
}

inline QString getText(QWidget *parent,
                       const QString &title,
                       const QString &label,
                       QLineEdit::EchoMode mode = QLineEdit::Normal,
                       const QString &text = QString(), bool *ok = nullptr)
{
    QInputDialog dialog(parent);
    dialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setTextEchoMode(mode);
    dialog.setTextValue(text);

    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (ok) *ok = accepted;
    return dialog.textValue();
}

} // namespace DialogUtils

#endif // DIALOGUTILS_H
