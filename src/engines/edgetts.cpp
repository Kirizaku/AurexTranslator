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

#include "edgetts.h"
#include "src/utils/pythonenv.h"

#include <QDir>
#include <QJsonDocument>

EdgeTts::EdgeTts(QObject *parent)
    : TtsEngine(parent)
{
    connect(this, &TtsEngine::voicesAvailable, this, &EdgeTts::chooseFirstVoice);
}

QString EdgeTts::summary() const
{
    return tr("Speaks through a Microsoft service. Every phrase is sent over the "
              "internet, and nothing is spoken without a connection.");
}

QStringList EdgeTts::availableVoices() const
{
    return m_catalog.keys();
}

QString EdgeTts::voiceLabel(const QString &voice) const
{
    QString label =
        QStringLiteral("%1 — %2").arg(languageLabel(localeOf(voice)), shortName(voice));

    const QString gender = m_catalog.value(voice)
                               .toObject()
                               .value(QStringLiteral("gender"))
                               .toString();

    if (gender == QLatin1String("Male"))
        return tr("%1, male").arg(label);

    if (gender == QLatin1String("Female"))
        return tr("%1, female").arg(label);

    return label;
}

void EdgeTts::loadSettings(const QJsonObject &settings)
{
    TtsEngine::loadSettings(settings);

    if (m_catalog.isEmpty())
        loadCatalog();
}

QStringList EdgeTts::serverArguments(const QString &voice) const
{
    QStringList args{serverScriptPath()};

    if (!voice.isEmpty())
        args << QStringLiteral("--voice") << voice;

    return args;
}

QString EdgeTts::serverWorkingDirectory() const
{
    return PythonEnv::dataDir(QStringLiteral("edge"));
}

QString EdgeTts::voiceFromInfo(const QJsonObject &info) const
{
    return info.value(QStringLiteral("name")).toString();
}

QJsonObject EdgeTts::synthesisPayload(const QString &text) const
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

QStringList EdgeTts::adoptVoiceList(const QJsonObject &voices)
{
    if (voices.isEmpty())
        return {};

    m_catalog = voices;

    if (mode() == Mode::Managed)
        saveCatalog();

    return m_catalog.keys();
}

bool EdgeTts::looksLikeAudio(const QByteArray &data) const
{
    if (data.startsWith("ID3"))
        return true;

    return data.size() > 2 && static_cast<uchar>(data[0]) == 0xFF && (static_cast<uchar>(data[1]) & 0xE0) == 0xE0;
}

bool EdgeTts::prepareManagedStart()
{
    return extractServerScript(QStringLiteral(":/python/edge_server.py"), serverScriptPath());
}

QString EdgeTts::localeOf(const QString &voice) const
{
    const QString stored = m_catalog.value(voice).toObject().value(QStringLiteral("locale")).toString();

    if (!stored.isEmpty())
        return stored;

    const QStringList parts = voice.split(QLatin1Char('-'));
    return parts.size() >= 2 ? parts.mid(0, 2).join(QLatin1Char('-')) : voice;
}

QString EdgeTts::shortName(const QString &voice) const
{
    const QString locale = localeOf(voice);

    QString name = voice;
    if (name.startsWith(locale + QLatin1Char('-')))
        name = name.mid(locale.size() + 1);

    if (name.endsWith(QStringLiteral("Neural")))
        name.chop(6);

    return name;
}

QString EdgeTts::languageLabel(const QString &code)
{
    if (code.isEmpty())
        return code;

    QString tag = code;
    const QLocale locale(tag.replace(QLatin1Char('-'), QLatin1Char('_')));

    const QString language = locale.nativeLanguageName();
    const QString territory = locale.nativeTerritoryName();

    if (language.isEmpty() || locale.language() == QLocale::C || code.count(QLatin1Char('-')) > 1)
        return code;

    return territory.isEmpty() ? language : QStringLiteral("%1 (%2)").arg(language, territory);
}

void EdgeTts::chooseFirstVoice(const QStringList &names)
{
    if (!voice().isEmpty() || names.isEmpty())
        return;

    QString system = QLocale::system().name();
    const QString locale = system.replace(QLatin1Char('_'), QLatin1Char('-'));
    const QString language = locale.section(QLatin1Char('-'), 0, 0);

    const QStringList wanted{locale + QLatin1Char('-'), language + QLatin1Char('-')};

    for (const QString &prefix : wanted) {
        for (const QString &name : names) {
            if (!name.startsWith(prefix))
                continue;

            setVoice(name);
            return;
        }
    }
}

void EdgeTts::loadCatalog()
{
    QFile file(catalogPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    m_catalog = QJsonDocument::fromJson(file.readAll()).object();
}

void EdgeTts::saveCatalog() const
{
    const QString path = catalogPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(m_catalog).toJson(QJsonDocument::Compact));
}

QString EdgeTts::catalogPath()
{
    return PythonEnv::dataDir(QStringLiteral("edge")) + QStringLiteral("/voices.json");
}

QString EdgeTts::serverScriptPath()
{
    return PythonEnv::dataDir(QStringLiteral("edge")) + QStringLiteral("/edge_server.py");
}
