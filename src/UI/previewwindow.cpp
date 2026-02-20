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

#include "previewwindow.h"

PreviewWindow::PreviewWindow(QLabel *parent)
    : QLabel{parent}
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    setAlignment(Qt::AlignCenter);
}

void PreviewWindow::setCurrentFrame(const QImage &frame)
{
    m_image = frame;
    m_image.setDevicePixelRatio(this->devicePixelRatio());

    if (!frame.isNull())
        setPixmap(QPixmap::fromImage(m_image).scaled(this->size() * this->devicePixelRatio(), Qt::KeepAspectRatio));
}

void PreviewWindow::clearFrame()
{
    clear();
    m_image = QImage();
}
