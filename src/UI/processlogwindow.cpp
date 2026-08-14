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

#include "processlogwindow.h"

#include <QApplication>
#include <QLabel>
#include <QClipboard>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>

ProcessLogWindow::ProcessLogWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(tr("Setup log"));
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(760, 460);

    m_stepLabel = new QLabel(this);
    m_stepLabel->setWordWrap(true);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->hide();

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_abortButton = new QPushButton(tr("Cancel"), this);
    m_abortButton->setEnabled(false);

    QPushButton *copyButton = new QPushButton(tr("Copy log"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(copyButton);
    buttons->addStretch();
    buttons->addWidget(m_abortButton);
    buttons->addWidget(m_closeButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_stepLabel);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_log, 1);
    layout->addLayout(buttons);

    connect(copyButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_log->toPlainText());
    });

    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::hide);
    connect(m_abortButton, &QPushButton::clicked, this, [this] {
        m_abortButton->setEnabled(false);
        appendLine(tr("--- cancelling ---"));
        emit abortRequested();
    });
}

void ProcessLogWindow::beginJob(const QString &title)
{
    m_running = true;

    m_stepLabel->setText(title);
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_abortButton->setEnabled(true);

    appendLine(QStringLiteral("=== %1 ===").arg(title));

    show();
    raise();
    activateWindow();
}

void ProcessLogWindow::endJob(bool ok, const QString &message)
{
    m_running = false;

    m_progressBar->hide();
    m_abortButton->setEnabled(false);

    const QString text = ok ? tr("Done") : tr("Failed: %1").arg(message);
    m_stepLabel->setText(text);
    appendLine(QStringLiteral("=== %1 ===").arg(text));

    if (!ok) {
        show();
        raise();
    }
}

void ProcessLogWindow::appendLine(const QString &line)
{
    QScrollBar *scrollBar = m_log->verticalScrollBar();
    const bool atBottom = scrollBar->value() >= scrollBar->maximum() - 4;

    m_log->appendPlainText(line);

    if (atBottom)
        scrollBar->setValue(scrollBar->maximum());
}

void ProcessLogWindow::setStep(const QString &title)
{
    m_stepLabel->setText(title);
    appendLine(QStringLiteral("--- %1 ---").arg(title));
}

void ProcessLogWindow::setProgress(qint64 received, qint64 total)
{
    if (!m_running)
        return;

    if (total <= 0) {
        m_progressBar->setRange(0, 0);
        return;
    }

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(static_cast<int>(received * 100 / total));
}
