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

#ifndef PROCESSLOGWINDOW_H
#define PROCESSLOGWINDOW_H

#include <QWidget>

class QPlainTextEdit;
class QProgressBar;
class QLabel;
class QPushButton;

class ProcessLogWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessLogWindow(QWidget *parent = nullptr);

    void beginJob(const QString &title);
    void endJob(bool ok, const QString &message);

public slots:
    void appendLine(const QString &line);
    void setStep(const QString &title);
    void setProgress(qint64 received, qint64 total);

signals:
    void abortRequested();

private:
    QLabel *m_stepLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_abortButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    bool m_running = false;
};

#endif // PROCESSLOGWINDOW_H
