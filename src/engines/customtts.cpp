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

#include "customtts.h"
#include "src/UI/customsettingsdialog.h"

CustomTts::CustomTts(QObject *parent)
    : TtsEngine(parent)
{
    setUseExternalServer(true);
}

QString CustomTts::summary() const
{
    return tr("Speaks through a TTS server you run and point it to. Only the "
              "text is sent, and whatever the server returns is played back.");
}

QDialog *CustomTts::createSettingsDialog(QWidget *parent)
{
    return new CustomSettingsDialog(this, parent);
}

void CustomTts::loadSettings(const QJsonObject &settings)
{
    TtsEngine::loadSettings(settings);
    setUseExternalServer(true);
}

QStringList CustomTts::serverArguments(const QString &voice) const
{
    Q_UNUSED(voice)
    return {};
}

QString CustomTts::serverWorkingDirectory() const
{
    return {};
}

QString CustomTts::voiceFromInfo(const QJsonObject &info) const
{
    return info.value(QStringLiteral("name")).toString();
}

QJsonObject CustomTts::synthesisPayload(const QString &text) const
{
    QJsonObject payload;
    payload.insert(QStringLiteral("text"), text);

    if (!voice().isEmpty())
        payload.insert(QStringLiteral("voice"), voice());

    if (speed() != kNormalSpeed) {
        const int delta = speed() - kNormalSpeed;
        const QString sign = delta > 0 ? QStringLiteral("+") : QString();

        payload.insert(QStringLiteral("rate"), sign + QString::number(delta) + QLatin1Char('%'));
    }

    return payload;
}

bool CustomTts::looksLikeAudio(const QByteArray &data) const
{
    if (data.isEmpty())
        return false;

    const char first = data.at(0);
    return first != '{' && first != '[' && first != '<';
}
