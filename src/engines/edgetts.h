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

#ifndef EDGETTS_H
#define EDGETTS_H

#include "ttsengine.h"

class EdgeTts : public TtsEngine
{
    Q_OBJECT

public:
    explicit EdgeTts(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("edge"); }
    QString name() const override { return QStringLiteral("Edge TTS"); }

    Kind kind() const override { return Kind::Online; }
    QString summary() const override;

    QStringList availableVoices() const override;
    QString voiceLabel(const QString &voice) const override;

    bool needsVoiceToStart() const override { return false; }

    void loadSettings(const QJsonObject &settings) override;

protected:
    QStringList serverArguments(const QString &voice) const override;
    QString serverWorkingDirectory() const override;
    QString voiceFromInfo(const QJsonObject &info) const override;
    QJsonObject synthesisPayload(const QString &text) const override;
    QStringList adoptVoiceList(const QJsonObject &voices) override;
    bool looksLikeAudio(const QByteArray &data) const override;
    bool prepareManagedStart() override;

    bool voiceChangeNeedsRestart(const QString &, const QString &) const override { return false; }
    bool wantsServerVoiceList() const override { return true; }

private:
    QString localeOf(const QString &voice) const;
    QString shortName(const QString &voice) const;
    static QString languageLabel(const QString &locale);

    void chooseFirstVoice(const QStringList &names);
    void loadCatalog();
    void saveCatalog() const;
    static QString catalogPath();
    static QString serverScriptPath();

    QJsonObject m_catalog;
};

#endif // EDGETTS_H
