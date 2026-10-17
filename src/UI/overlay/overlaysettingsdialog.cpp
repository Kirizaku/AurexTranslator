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

#include "overlaysettingsdialog.h"
#include "ui_overlaysettingsdialog.h"

#include <QHideEvent>
#include <QShowEvent>

OverlaySettingsDialog::OverlaySettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OverlaySettingsDialog)
{
    ui->setupUi(this);

    const QList<QPair<QSlider *, QSpinBox *>> pixelRows = {
        { ui->overlayCornerRadiusSlider, ui->overlayCornerRadiusSpin },
        { ui->overlayMarginTopSlider,    ui->overlayMarginTopSpin },
        { ui->overlayMarginBottomSlider, ui->overlayMarginBottomSpin },
        { ui->overlayMarginLeftSlider,   ui->overlayMarginLeftSpin },
        { ui->overlayMarginRightSlider,  ui->overlayMarginRightSpin },
    };
    for (const auto &[slider, spin] : pixelRows) {
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
    }

    const QList<QPair<QSlider *, QDoubleSpinBox *>> zoneRows = {
        { ui->overlayZoneXSlider, ui->overlayZoneXSpin },
        { ui->overlayZoneYSlider, ui->overlayZoneYSpin },
        { ui->overlayZoneWSlider, ui->overlayZoneWSpin },
        { ui->overlayZoneHSlider, ui->overlayZoneHSpin },
    };
    for (const auto &[slider, spin] : zoneRows) {
        connect(slider, &QSlider::valueChanged, spin, [spin](int v) {
            const double d = v / 100.0;
            if (qAbs(spin->value() - d) > 0.001) spin->setValue(d);
        });
        connect(spin, &QDoubleSpinBox::valueChanged, slider, [slider](double v) {
            slider->setValue(qRound(v * 100.0));
        });
    }
}

OverlaySettingsDialog::~OverlaySettingsDialog()
{
    delete ui;
}

void OverlaySettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    emit previewRequested();
}

void OverlaySettingsDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    emit previewFinished();
}
