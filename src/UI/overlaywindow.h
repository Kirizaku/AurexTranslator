/******************************************************************************
    Copyright (C) 2025 by Daniil Nabiulin

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

#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <QLabel>

class OverlayWindow : public QLabel
{
    Q_OBJECT

public:
    explicit OverlayWindow(QWidget *parent = nullptr);
    bool getIsRectBrushEmpty() const { return m_rectBrush.isEmpty(); }
    void setInnerBrushActive(bool value) { m_innerBrushActive = value; if (!value) m_innerRectBrush.setRect(0,0,0,0); }
    void clearFrame() { this->clear(); }

signals:
    void currentRoi(QRect currentRect);
    void currentInnerRoi(QRect currentInnerRect);
    void hideOverlay();

public slots:
    void updateRectBrush(int width, int height);

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QColor m_overlayColor = QColor(0, 0, 0, 0);
    QRect m_rectBrush = QRect(0,0,0,0);
    QRect m_originalRect = QRect(0,0,0,0);
    QPoint m_dragStartPosition = QPoint(-1, -1);

    bool m_resizeLeft = false;
    bool m_resizeRight = false;
    bool m_resizeTop = false;
    bool m_resizeBottom = false;

    QPoint m_selectStart, m_selectEnd, m_fixedCorner = QPoint(-1, -1);
    bool m_isPaint = false;
    bool m_isResizing = false;
    bool m_isMoving = false;

    QRect m_innerRectBrush = QRect(0,0,0,0);
    bool m_innerBrushActive = false;
    bool m_isPaintInner = false;
    bool m_isMovingInner = false;
    bool m_isResizingInner = false;

    void updateCursorShape(const QPoint& pos);
};

#endif // OVERLAYWINDOW_H
