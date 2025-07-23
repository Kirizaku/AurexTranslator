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

    #include "overlaywindow.h"
    #include <QMouseEvent>
    #include <QPainter>

    OverlayWindow::OverlayWindow(QWidget *parent)
        : QLabel{parent}
    {
        setMouseTracking(true);
        setAlignment(Qt::AlignmentFlag::AlignCenter);
        m_overlayColor = QColor(0,0,0,100);

        setAttribute(Qt::WA_TranslucentBackground, true);
        setWindowFlags(Qt::WindowStaysOnTopHint |
                       Qt::FramelessWindowHint
                       );
    }

    void OverlayWindow::showEvent(QShowEvent *event)
    {
        activateWindow();

        int imageX = (width() - pixmap().width()) / 2;
        int imageY = (height() - pixmap().height()) / 2;

        if (m_rectBrush.isValid() && !QRect(imageX, imageY, pixmap().width(), pixmap().height()).contains(m_rectBrush)) {
            m_rectBrush = QRect();
        }
    }

    void OverlayWindow::updateRectBrush(int newwidth, int newheight)
    {
        if (!m_rectBrush.isEmpty()) {
            int imageX = (width() - newwidth) / 2;
            int imageY = (height() - newheight) / 2;

            QRect parentRect = QRect(imageX, imageY, newwidth, newheight);

            if (m_rectBrush.x() < parentRect.x()) {
                m_rectBrush.moveLeft(parentRect.x());
            }
            if (m_rectBrush.y() < parentRect.y()) {
                m_rectBrush.moveTop(parentRect.y());
            }
            if (m_rectBrush.right() > parentRect.right()) {
                m_rectBrush.setWidth(parentRect.right() - m_rectBrush.x());
            }
            if (m_rectBrush.bottom() > parentRect.bottom()) {
                m_rectBrush.setHeight(parentRect.bottom() - m_rectBrush.y());
            }
        }

        if (!m_innerRectBrush.isEmpty()) {
            if (m_innerRectBrush.x() < m_rectBrush.x()) {
                m_innerRectBrush.moveLeft(m_rectBrush.x());
            }
            if (m_innerRectBrush.y() < m_rectBrush.y()) {
                m_innerRectBrush.moveTop(m_rectBrush.y());
            }
            if (m_innerRectBrush.right() > m_rectBrush.right()) {
                m_innerRectBrush.setWidth(m_rectBrush.right() - m_innerRectBrush.x());
            }
            if (m_innerRectBrush.bottom() > m_rectBrush.bottom()) {
                m_innerRectBrush.setHeight(m_rectBrush.bottom() - m_innerRectBrush.y());
            }
        }
        emit currentRoi(m_rectBrush);
        if (!m_innerRectBrush.isEmpty()) {
            emit currentInnerRoi(m_innerRectBrush);
        }
        update();
    }

    void OverlayWindow::paintEvent(QPaintEvent* event)
    {
        QLabel::paintEvent(event);
        QPainter painter(this);

        const int handleSize = 5;
        int imageX = (width() - pixmap().width()) / 2;
        int imageY = (height() - pixmap().height()) / 2;

        painter.fillRect(0, 0, width(), imageY, Qt::black);
        painter.fillRect(0, imageY, imageX, pixmap().height(), Qt::black);
        painter.fillRect(imageX + pixmap().width(), imageY, width() - imageX - pixmap().width(), pixmap().height(), Qt::black);
        painter.fillRect(0, imageY + pixmap().height(), width(), height() - imageY - pixmap().height(), Qt::black);

        if (m_rectBrush.isEmpty()) {
            painter.fillRect(this->rect(), m_overlayColor);
        } else {
            QRegion outerRegion(this->rect());
            QRegion innerRegion(m_rectBrush);
            QRegion transparentRegion = outerRegion.subtracted(innerRegion);

            painter.setClipRegion(transparentRegion);
            painter.fillRect(this->rect(), m_overlayColor);
            painter.setClipping(false);

            QPen pen(m_innerBrushActive ? Qt::gray : Qt::red, 2);
            painter.setPen(pen);
            painter.drawRect(m_rectBrush);

            if (!m_innerBrushActive) {
                painter.setBrush(Qt::red);
                painter.drawEllipse(m_rectBrush.topLeft(), handleSize, handleSize);
                painter.drawEllipse(m_rectBrush.topRight(), handleSize, handleSize);
                painter.drawEllipse(m_rectBrush.bottomLeft(), handleSize, handleSize);
                painter.drawEllipse(m_rectBrush.bottomRight(), handleSize, handleSize);

                painter.drawEllipse((m_rectBrush.topLeft() + m_rectBrush.topRight()) / 2, handleSize, handleSize);
                painter.drawEllipse((m_rectBrush.bottomLeft() + m_rectBrush.bottomRight()) / 2, handleSize, handleSize);
                painter.drawEllipse((m_rectBrush.topLeft() + m_rectBrush.bottomLeft()) / 2, handleSize, handleSize);
                painter.drawEllipse((m_rectBrush.topRight() + m_rectBrush.bottomRight()) / 2, handleSize, handleSize);
            }

            if (!m_innerRectBrush.isEmpty()) {
                QPen innerPen(Qt::yellow, 2);
                painter.setPen(innerPen);
                painter.drawRect(m_innerRectBrush);
                painter.fillRect(m_innerRectBrush, Qt::black);
                painter.setBrush(Qt::yellow);
                painter.drawEllipse(m_innerRectBrush.topLeft(), handleSize, handleSize);
                painter.drawEllipse(m_innerRectBrush.topRight(), handleSize, handleSize);
                painter.drawEllipse(m_innerRectBrush.bottomLeft(), handleSize, handleSize);
                painter.drawEllipse(m_innerRectBrush.bottomRight(), handleSize, handleSize);

                painter.drawEllipse((m_innerRectBrush.topLeft() + m_innerRectBrush.topRight()) / 2, handleSize, handleSize);
                painter.drawEllipse((m_innerRectBrush.bottomLeft() + m_innerRectBrush.bottomRight()) / 2, handleSize, handleSize);
                painter.drawEllipse((m_innerRectBrush.topLeft() + m_innerRectBrush.bottomLeft()) / 2, handleSize, handleSize);
                painter.drawEllipse((m_innerRectBrush.topRight() + m_innerRectBrush.bottomRight()) / 2, handleSize, handleSize);
            }
        }
    }

    void OverlayWindow::mousePressEvent(QMouseEvent *event)
    {
        QLabel::mousePressEvent(event);

        if (event->button() == Qt::LeftButton) {
            QPoint pos = event->pos();

            if (m_innerBrushActive && !m_rectBrush.isEmpty()) {
                m_resizeLeft = m_resizeRight = m_resizeTop = m_resizeBottom = false;

                const int margin = 10;

                if (pos.x() >= (m_innerRectBrush.left() - margin) && pos.x() <= m_innerRectBrush.left() + margin &&
                    pos.y() >= (m_innerRectBrush.top() - margin) && pos.y() <= (m_innerRectBrush.bottom() + margin)) {
                    m_resizeLeft = true;
                    m_fixedCorner = m_innerRectBrush.topRight();
                }

                if (pos.x() >= m_innerRectBrush.right() - margin && pos.x() <= (m_innerRectBrush.right() + margin) &&
                    pos.y() >= (m_innerRectBrush.top() - margin) && pos.y() <= (m_innerRectBrush.bottom() + margin)) {
                    m_fixedCorner = m_innerRectBrush.topLeft();
                    m_resizeRight = true;
                }

                if (pos.y() >= (m_innerRectBrush.top() - margin) && pos.y() <= m_innerRectBrush.top() + margin &&
                    pos.x() >= (m_innerRectBrush.left() - margin) && pos.x() <= (m_innerRectBrush.right() + margin)) {
                    m_fixedCorner = m_innerRectBrush.bottomLeft();
                    m_resizeTop = true;
                }

                if (pos.y() >= m_innerRectBrush.bottom() - margin && pos.y() <= (m_innerRectBrush.bottom() + margin) &&
                    pos.x() >= (m_innerRectBrush.left() - margin) && pos.x() <= (m_innerRectBrush.right() + margin)) {
                    m_resizeBottom = true;
                    m_fixedCorner = m_innerRectBrush.topLeft();
                }

                if (m_resizeLeft || m_resizeRight || m_resizeTop || m_resizeBottom) {
                    m_isResizingInner = true;
                    m_dragStartPosition = event->pos();
                    updateCursorShape(pos);
                    m_originalRect = m_innerRectBrush;
                    return;
                }

                if (m_innerRectBrush.contains(event->pos())) {
                    m_isMovingInner = true;
                    m_dragStartPosition = event->pos() - m_innerRectBrush.topLeft();
                    m_originalRect = m_innerRectBrush;
                } else {
                    m_selectStart = event->pos();
                    m_isPaintInner = true;
                }
            } else {
                m_resizeLeft = m_resizeRight = m_resizeTop = m_resizeBottom = false;

                const int margin = 10;

                if (pos.x() >= (m_rectBrush.left() - margin) && pos.x() <= m_rectBrush.left() + margin &&
                    pos.y() >= (m_rectBrush.top() - margin) && pos.y() <= (m_rectBrush.bottom() + margin)) {
                    m_resizeLeft = true;
                    m_fixedCorner = m_rectBrush.topRight();
                }

                if (pos.x() >= m_rectBrush.right() - margin && pos.x() <= (m_rectBrush.right() + margin) &&
                    pos.y() >= (m_rectBrush.top() - margin) && pos.y() <= (m_rectBrush.bottom() + margin)) {
                    m_fixedCorner = m_rectBrush.topLeft();
                    m_resizeRight = true;
                }

                if (pos.y() >= (m_rectBrush.top() - margin) && pos.y() <= m_rectBrush.top() + margin &&
                    pos.x() >= (m_rectBrush.left() - margin) && pos.x() <= (m_rectBrush.right() + margin)) {
                    m_fixedCorner = m_rectBrush.bottomLeft();
                    m_resizeTop = true;
                }

                if (pos.y() >= m_rectBrush.bottom() - margin && pos.y() <= (m_rectBrush.bottom() + margin) &&
                    pos.x() >= (m_rectBrush.left() - margin) && pos.x() <= (m_rectBrush.right() + margin)) {
                    m_resizeBottom = true;
                    m_fixedCorner = m_rectBrush.topLeft();
                }

                if (m_resizeLeft || m_resizeRight || m_resizeTop || m_resizeBottom) {
                    m_isResizing = true;
                    m_dragStartPosition = event->pos();
                    updateCursorShape(pos);
                    m_originalRect = m_rectBrush;
                    return;
                }

                if (m_rectBrush.contains(event->pos())) {
                    m_isMoving = true;
                    m_dragStartPosition = event->pos() - m_rectBrush.topLeft();
                    m_originalRect = m_rectBrush;
                } else {
                    m_selectStart = event->pos();
                    m_isPaint = true;
                }
            }
            update();
        }
    }

    void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
    {
        int imageX = (width() - pixmap().width()) / 2;
        int imageY = (height() - pixmap().height()) / 2;

        QPoint pixmapPos = event->pos() - QPoint(imageX, imageY);
        bool isInsidePixmap = !pixmap().isNull() &&
                              pixmapPos.x() >= 0 && pixmapPos.y() >= 0 &&
                              pixmapPos.x() <= pixmap().width() &&
                              pixmapPos.y() <= pixmap().height();

        if (m_innerBrushActive && !m_rectBrush.isEmpty()) {
            if (m_isPaintInner) {
                m_selectEnd = event->pos();
                QRect newRect = QRect(m_selectStart, m_selectEnd).normalized();
                QRect parentRect = m_rectBrush;

                newRect = newRect.intersected(parentRect);

                if (newRect.width() < 1) {
                    newRect.setWidth(1);
                }
                if (newRect.height() < 1) {
                    newRect.setHeight(1);
                }
                m_innerRectBrush = newRect;
            }

            if (m_isResizingInner) {
                QPoint delta = event->pos() - m_dragStartPosition;
                QRect newRect = m_originalRect;
                QRect parentRect = m_rectBrush;

                if (m_resizeLeft) {
                    newRect.setLeft(qMax(parentRect.left(), qMin(parentRect.right(), m_originalRect.left() + delta.x())));
                    newRect = newRect.normalized();
                }

                if (m_resizeRight) {
                    newRect.setRight(qMin(parentRect.right(), qMax(parentRect.left(), m_originalRect.right() + delta.x())));
                    newRect = newRect.normalized();
                }

                if (m_resizeTop) {
                    newRect.setTop(qMax(parentRect.top(), qMin(parentRect.bottom(), m_originalRect.top() + delta.y())));
                    newRect = newRect.normalized();
                }

                if (m_resizeBottom) {
                    newRect.setBottom(qMin(parentRect.bottom(), qMax(parentRect.top(), m_originalRect.bottom() + delta.y())));
                    newRect = newRect.normalized();
                }
                m_innerRectBrush = newRect;
            }

            if (m_isMovingInner) {
                QPoint newPos = event->pos() - m_dragStartPosition;
                QRect parentRect = m_rectBrush;

                newPos.setX(qMax(parentRect.left(), qMin(parentRect.right() - m_innerRectBrush.width(), newPos.x())));
                newPos.setY(qMax(parentRect.top(), qMin(parentRect.bottom() - m_innerRectBrush.height(), newPos.y())));
                m_innerRectBrush.moveTopLeft(newPos);
            }
        } else {
            if (m_isPaint && isInsidePixmap) {
                m_selectEnd = event->pos();
                QRect newRect = QRect(m_selectStart, m_selectEnd).normalized();
                QRect parentRect = QRect(imageX, imageY, pixmap().width(), pixmap().height());

                newRect = newRect.intersected(parentRect);

                if (newRect.width() < 1) {
                    newRect.setWidth(1);
                }
                if (newRect.height() < 1) {
                    newRect.setHeight(1);
                }
                m_rectBrush = newRect;
            }

            if (m_isResizing) {
                QPoint delta = event->pos() - m_dragStartPosition;
                QRect newRect = m_originalRect;
                QRect parentRect = QRect(imageX, imageY, pixmap().width(), pixmap().height());

                if (m_resizeLeft) {
                    newRect.setLeft(qMax(imageX, qMin(parentRect.width(), m_originalRect.left() + delta.x())));
                    newRect = newRect.normalized();
                }

                if (m_resizeRight) {
                    newRect.setRight(qMin(imageX + parentRect.width(), qMax(imageX, m_originalRect.right() + delta.x())));
                    newRect = newRect.normalized();
                }

                if (m_resizeTop) {
                    newRect.setTop(qMax(imageY, qMin(imageY + parentRect.height(), m_originalRect.top() + delta.y())));
                    newRect = newRect.normalized();
                }

                if (m_resizeBottom) {
                    newRect.setBottom(qMin(imageY + parentRect.height(), qMax(imageY, m_originalRect.bottom() + delta.y())));
                    newRect = newRect.normalized();
                }
                m_rectBrush = newRect;
            }

            if (m_isMoving) {
                QPoint newPos = event->pos() - m_dragStartPosition;
                QRect parentRect = QRect(imageX, imageY, pixmap().width(), pixmap().height());

                newPos.setX(qMax(imageX, qMin(imageX + parentRect.width() - m_rectBrush.width(), newPos.x())));
                newPos.setY(qMax(imageY, qMin(imageY + parentRect.height() - m_rectBrush.height(), newPos.y())));
                m_rectBrush.moveTopLeft(newPos);
            }
        }
        updateCursorShape(event->pos());
        update();
    }

    void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
    {
        if (m_innerBrushActive) {
            m_isPaintInner = false;
            if (m_isResizingInner) m_isResizingInner = false;
            if (m_isMovingInner) m_isMovingInner = false;
        } else {
            m_isPaint = false;
            if (m_isResizing) m_isResizing = false;
            if (m_isMoving) m_isMoving = false;
        }
        emit currentRoi(m_rectBrush);

        if (!m_innerRectBrush.isEmpty()) {
            emit currentInnerRoi(m_innerRectBrush);
        }
    }

    void OverlayWindow::keyPressEvent(QKeyEvent *event)
    {
        if (event->key() == Qt::Key_Escape) {
            emit hideOverlay();
        } else {
            QWidget::keyPressEvent(event);
        }
    }

    void OverlayWindow::updateCursorShape(const QPoint& pos)
    {
        const int margin = 10;

        if (m_innerBrushActive && !m_innerRectBrush.isEmpty()) {
            bool left = (pos.x() >= (m_innerRectBrush.left() - margin) && pos.x() <= m_innerRectBrush.left() + margin &&
                         pos.y() >= (m_innerRectBrush.top() - margin) && pos.y() <= (m_innerRectBrush.bottom() + margin));

            bool right = (pos.x() >= m_innerRectBrush.right() - margin && pos.x() <= (m_innerRectBrush.right() + margin) &&
                          pos.y() >= (m_innerRectBrush.top() - margin) && pos.y() <= (m_innerRectBrush.bottom() + margin));

            bool top = (pos.y() >= (m_innerRectBrush.top() - margin) && pos.y() <= m_innerRectBrush.top() + margin &&
                        pos.x() >= (m_innerRectBrush.left() - margin) && pos.x() <= (m_innerRectBrush.right() + margin));

            bool bottom = (pos.y() >= m_innerRectBrush.bottom() - margin && pos.y() <= (m_innerRectBrush.bottom() + margin) &&
                           pos.x() >= (m_innerRectBrush.left() - margin) && pos.x() <= (m_innerRectBrush.right() + margin));

            if ((left && top) || (right && bottom)) {
                setCursor(Qt::SizeFDiagCursor);
            }
            else if ((right && top) || (left && bottom)) {
                setCursor(Qt::SizeBDiagCursor);
            }
            else if (left || right) {
                setCursor(Qt::SizeHorCursor);
            }
            else if (top || bottom) {
                setCursor(Qt::SizeVerCursor);
            }
            else if (m_isMovingInner) {
                setCursor(Qt::OpenHandCursor);
            }
            else if (!m_innerRectBrush.contains(pos)) {
                setCursor(Qt::CrossCursor);
            }
            else {
                setCursor(Qt::ArrowCursor);
            }
        } else if (!m_innerBrushActive && !m_rectBrush.isEmpty()) {
            bool left = (pos.x() >= (m_rectBrush.left() - margin) && pos.x() <= m_rectBrush.left() + margin &&
                         pos.y() >= (m_rectBrush.top() - margin) && pos.y() <= (m_rectBrush.bottom() + margin));

            bool right = (pos.x() >= m_rectBrush.right() - margin && pos.x() <= (m_rectBrush.right() + margin) &&
                          pos.y() >= (m_rectBrush.top() - margin) && pos.y() <= (m_rectBrush.bottom() + margin));

            bool top = (pos.y() >= (m_rectBrush.top() - margin) && pos.y() <= m_rectBrush.top() + margin &&
                        pos.x() >= (m_rectBrush.left() - margin) && pos.x() <= (m_rectBrush.right() + margin));

            bool bottom = (pos.y() >= m_rectBrush.bottom() - margin && pos.y() <= (m_rectBrush.bottom() + margin) &&
                           pos.x() >= (m_rectBrush.left() - margin) && pos.x() <= (m_rectBrush.right() + margin));

            if ((left && top) || (right && bottom)) {
                setCursor(Qt::SizeFDiagCursor);
            }
            else if ((right && top) || (left && bottom)) {
                setCursor(Qt::SizeBDiagCursor);
            }
            else if (left || right) {
                setCursor(Qt::SizeHorCursor);
            }
            else if (top || bottom) {
                setCursor(Qt::SizeVerCursor);
            }
            else if (m_isMoving) {
                setCursor(Qt::OpenHandCursor);
            }
            else if (!m_rectBrush.contains(pos)) {
                setCursor(Qt::CrossCursor);
            }
            else {
                setCursor(Qt::ArrowCursor);
            }
        } else {
            setCursor(Qt::CrossCursor);
        }
    }
