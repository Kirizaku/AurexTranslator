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

#include "hoverlabel.h"
#include <QPainter>

HoverLabel::HoverLabel(QWidget *parent, const QString &text)
    : QLabel(text, parent) {}

void HoverLabel::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QLabel::enterEvent(event);
}

void HoverLabel::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QLabel::leaveEvent(event);
}

void HoverLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    if (m_hovered) {
        QRect rect = contentsRect();
        int adjust = m_borderWidth / 2;
        rect.adjust(adjust, adjust, -adjust, -adjust);

        QPen pen(m_borderColor, m_borderWidth);
        QPainter painter(this);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
    }
}
