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

#include "overlayprocesspickerdialog.h"
#include "src/UI/widgets/processpickerwidget.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

OverlayProcessPickerDialog::OverlayProcessPickerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Game"));
    resize(640, 420);

    m_picker = new ProcessPickerWidget(this);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_picker, &ProcessPickerWidget::selectionChanged, this, [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_picker->selectedProcessName().isEmpty());
    });
    connect(m_picker, &ProcessPickerWidget::activated, this, [this, buttons] {
        if (buttons->button(QDialogButtonBox::Ok)->isEnabled()) accept();
    });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_picker, 1);
    layout->addWidget(buttons);

    m_picker->refresh();
}

QString OverlayProcessPickerDialog::selectedProcess() const
{
    return m_picker->selectedProcessName();
}
