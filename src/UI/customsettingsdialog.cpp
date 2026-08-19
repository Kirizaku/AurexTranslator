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

#include "customsettingsdialog.h"
#include "src/engines/customtts.h"

#include <QLineEdit>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

CustomSettingsDialog::CustomSettingsDialog(CustomTts *engine, QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
{
    setWindowTitle(engine->name());
    setMinimumSize(480, 200);

    m_serverUrl = new QLineEdit(engine->externalUrl());
    m_serverUrl->setPlaceholderText(QStringLiteral("http://127.0.0.1:5000"));
    m_checkButton = new QPushButton(tr("Check"));

    QHBoxLayout *serverLayout = new QHBoxLayout;
    serverLayout->addWidget(m_serverUrl);
    serverLayout->addWidget(m_checkButton);

    QFormLayout *serverForm = new QFormLayout;
    serverForm->addRow(tr("Server address"), serverLayout);

    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);

    QVBoxLayout *serverBoxLayout = new QVBoxLayout;
    serverBoxLayout->addLayout(serverForm);
    serverBoxLayout->addWidget(m_statusLabel);

    QGroupBox *serverBox = new QGroupBox(tr("Server"));
    serverBox->setLayout(serverBoxLayout);

    QLabel *label = new QLabel(tr("The server must answer /info, /voices and /synthesize. "
                                  "See the \"Custom TTS Server\" wiki page for details."));
    label->setWordWrap(true);
    label->setEnabled(false);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(serverBox);
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(m_serverUrl, &QLineEdit::textChanged, this, [this](const QString &url) { m_engine->setExternalUrl(url); });
    connect(m_checkButton, &QPushButton::clicked, this, [this] { m_engine->start(); });
    connect(m_engine, &CustomTts::stateChanged, this, [this](TtsEngine::State) { refreshState(); });
    connect(m_engine, &CustomTts::errorOccurred, this, [this](const QString &error) { setStatus(error); });

    refreshState();
}

void CustomSettingsDialog::refreshState()
{
    QString status;
    switch (m_engine->state()) {
    case TtsEngine::State::Stopped:
        status = tr("Not connected");
        break;
    case TtsEngine::State::Starting:
        status = tr("Connecting...");
        break;
    case TtsEngine::State::Ready:
        status = m_engine->voice().isEmpty()
                     ? tr("Ready at %1").arg(m_engine->baseUrl())
                     : tr("Ready at %1, voice %2").arg(m_engine->baseUrl(), m_engine->voiceLabel(m_engine->voice()));
        break;
    case TtsEngine::State::Failed:
        status = tr("Failed");
        break;
    }
    setStatus(status);
}

void CustomSettingsDialog::setStatus(const QString &text)
{
    m_statusLabel->setText(tr("Status: %1").arg(text));
}
