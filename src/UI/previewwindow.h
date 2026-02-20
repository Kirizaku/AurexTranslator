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

#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include <QObject>
#include <QLabel>

class PreviewWindow : public QLabel
{
    Q_OBJECT
public:
    explicit PreviewWindow(QLabel *parent = nullptr);

public slots:
    void setCurrentFrame(const QImage &frame);
    void clearFrame();

signals:

private:
    QImage m_image;
};

#endif // PREVIEWWINDOW_H
