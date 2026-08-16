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

#ifndef PIPERTTS_H
#define PIPERTTS_H

#include "ttsengine.h"

class PiperVoiceCatalog;
class PiperTts : public TtsEngine
{
    Q_OBJECT

public:
    explicit PiperTts(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("piper"); }
    QString name() const override { return QStringLiteral("Piper TTS"); }

    Kind kind() const override { return Kind::Offline; }
    QString summary() const override
    {
        return tr("Speaks on this machine. Voices are downloaded once, and no text is sent anywhere.");
    }

    QStringList availableVoices() const override;

    bool hasSettings() const override { return true; }
    QDialog *createSettingsDialog(QWidget *parent) override;

    QJsonObject saveSettings() const override;
    void loadSettings(const QJsonObject &settings) override;

    PiperVoiceCatalog *catalog() const { return m_catalog; }

protected:
    QStringList serverArguments(const QString &voice) const override;
    QString serverWorkingDirectory() const override;
    QString voiceFromInfo(const QJsonObject &info) const override;
    QJsonObject synthesisPayload(const QString &text) const override;

private:
    PiperVoiceCatalog *m_catalog = nullptr;
};

#endif // PIPERTTS_H
