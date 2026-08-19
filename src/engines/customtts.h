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

#ifndef CUSTOMTTS_H
#define CUSTOMTTS_H

#include "ttsengine.h"

class CustomTts : public TtsEngine
{
    Q_OBJECT

public:
    explicit CustomTts(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("custom"); }
    QString name() const override { return tr("Custom server"); }

    Kind kind() const override { return Kind::Custom; }
    QString summary() const override;

    QStringList availableVoices() const override { return m_serverVoices; }

    bool needsVoiceToStart() const override { return false; }

    bool hasSettings() const override { return true; }
    QDialog *createSettingsDialog(QWidget *parent) override;

    void loadSettings(const QJsonObject &settings) override;

protected:
    QStringList serverArguments(const QString &voice) const override;
    QString serverWorkingDirectory() const override;
    QString voiceFromInfo(const QJsonObject &info) const override;
    QJsonObject synthesisPayload(const QString &text) const override;
    bool looksLikeAudio(const QByteArray &data) const override;
};

#endif // CUSTOMTTS_H
