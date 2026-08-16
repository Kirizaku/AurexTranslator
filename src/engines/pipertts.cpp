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

#include "pipertts.h"

#include "src/UI/pipersettingsdialog.h"
#include "pipervoicecatalog.h"

PiperTts::PiperTts(QObject *parent)
    : TtsEngine(parent)
    , m_catalog(new PiperVoiceCatalog(this))
{
    connect(m_catalog, &PiperVoiceCatalog::logLine, this, &PiperTts::logLine);
}

QStringList PiperTts::availableVoices() const
{
    return useExternalServer() ? m_serverVoices : PiperVoiceCatalog::installedVoices();
}

QDialog *PiperTts::createSettingsDialog(QWidget *parent)
{
    return new PiperSettingsDialog(this, parent);
}

QJsonObject PiperTts::saveSettings() const
{
    QJsonObject settings = TtsEngine::saveSettings();
    settings.insert(QStringLiteral("voices_url"), PiperVoiceCatalog::configuredRepoBase());
    return settings;
}

void PiperTts::loadSettings(const QJsonObject &settings)
{
    TtsEngine::loadSettings(settings);
    PiperVoiceCatalog::setRepoBase(settings.value(QStringLiteral("voices_url")).toString());
}

QStringList PiperTts::serverArguments(const QString &voice) const
{
    return {QStringLiteral("-m"),
            QStringLiteral("piper.http_server"),
            QStringLiteral("--model"), voice,
            QStringLiteral("--data-dir"), PiperVoiceCatalog::voicesDir()};
}

QString PiperTts::serverWorkingDirectory() const
{
    return PiperVoiceCatalog::voicesDir();
}

QString PiperTts::voiceFromInfo(const QJsonObject &info) const
{
    return info.value(QStringLiteral("voice"))
        .toObject()
        .value(QStringLiteral("name"))
        .toString();
}

QJsonObject PiperTts::synthesisPayload(const QString &text) const
{
    QJsonObject payload;
    payload.insert(QStringLiteral("text"), text);

    if (mode() == Mode::External && !voice().isEmpty())
        payload.insert(QStringLiteral("voice"), voice());

    if (speed() != kNormalSpeed)
        payload.insert(QStringLiteral("length_scale"),
                       double(kNormalSpeed) / double(speed()));

    return payload;
}
