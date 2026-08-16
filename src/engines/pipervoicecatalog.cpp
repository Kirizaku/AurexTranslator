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

#include "pipervoicecatalog.h"
#include "src/utils/pythonenv.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace {

const char *kRepoBase = "https://huggingface.co/rhasspy/piper-voices/resolve/main/";
QString g_repoBase;

QNetworkRequest makeRequest(const QString &url)
{
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout();
    return request;
}

} // namespace

PiperVoiceCatalog::PiperVoiceCatalog(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString PiperVoiceCatalog::voicesDir()
{
    return PythonEnv::dataDir(QStringLiteral("piper-voices"));
}

QString PiperVoiceCatalog::defaultRepoBase()
{
    return QLatin1String(kRepoBase);
}

QString PiperVoiceCatalog::repoBase()
{
    return g_repoBase.isEmpty() ? defaultRepoBase() : g_repoBase;
}

QString PiperVoiceCatalog::configuredRepoBase()
{
    return g_repoBase;
}

void PiperVoiceCatalog::setRepoBase(const QString &base)
{
    const QString trimmed = base.trimmed();

    if (trimmed.isEmpty() || trimmed == defaultRepoBase()) {
        g_repoBase.clear();
        return;
    }

    g_repoBase = trimmed.endsWith(QLatin1Char('/')) ? trimmed : trimmed + QLatin1Char('/');
}

QString PiperVoiceCatalog::voicesJsonUrl()
{
    return repoBase() + QStringLiteral("voices.json?download=true");
}

QString PiperVoiceCatalog::fileUrl(const QString &repoPath)
{
    return repoBase() + repoPath + QStringLiteral("?download=true");
}

void PiperVoiceCatalog::refresh()
{
    if (m_refreshReply)
        return;

    emit logLine(QStringLiteral("Fetching the voice list: %1").arg(voicesJsonUrl()));

    QNetworkReply *reply = m_network->get(makeRequest(voicesJsonUrl()));
    m_refreshReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_refreshReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            emit refreshed(false, reply->errorString());
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit refreshed(false, parseError.errorString());
            return;
        }

        m_voices.clear();

        const QJsonObject root = document.object();
        for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
            const QJsonObject entry = it.value().toObject();

            PiperVoice voice;
            voice.key = entry.value(QStringLiteral("key")).toString(it.key());
            voice.name = entry.value(QStringLiteral("name")).toString();
            voice.quality = entry.value(QStringLiteral("quality")).toString();
            voice.numSpeakers = entry.value(QStringLiteral("num_speakers")).toInt(1);

            const QJsonObject language = entry.value(QStringLiteral("language")).toObject();
            voice.languageCode = language.value(QStringLiteral("code")).toString();
            voice.languageName = language.value(QStringLiteral("name_native")).toString();
            if (voice.languageName.isEmpty())
                voice.languageName = language.value(QStringLiteral("name_english")).toString();

            const QJsonObject files = entry.value(QStringLiteral("files")).toObject();
            for (auto file = files.constBegin(); file != files.constEnd(); ++file) {
                const qint64 size = static_cast<qint64>(
                    file.value().toObject().value(QStringLiteral("size_bytes")).toDouble());

                if (file.key().endsWith(QStringLiteral(".onnx"))) {
                    voice.modelPath = file.key();
                    voice.sizeBytes += size;
                } else if (file.key().endsWith(QStringLiteral(".onnx.json"))) {
                    voice.configPath = file.key();
                    voice.sizeBytes += size;
                }
            }

            if (voice.isValid())
                m_voices.append(voice);
        }

        std::sort(m_voices.begin(), m_voices.end(), [](const PiperVoice &a, const PiperVoice &b) {
            if (a.languageCode != b.languageCode)
                return a.languageCode < b.languageCode;
            return a.key < b.key;
        });

        emit logLine(QStringLiteral("Voices available: %1").arg(m_voices.size()));
        emit refreshed(true, {});
    });
}

QStringList PiperVoiceCatalog::languages() const
{
    QStringList codes;
    for (const PiperVoice &voice : m_voices)
        if (!codes.contains(voice.languageCode))
            codes << voice.languageCode;

    return codes;
}

PiperVoice PiperVoiceCatalog::voice(const QString &key) const
{
    for (const PiperVoice &voice : m_voices)
        if (voice.key == key)
            return voice;

    return {};
}

bool PiperVoiceCatalog::isInstalled(const QString &key)
{
    const QString model = modelFilePath(key);
    return QFileInfo::exists(model) && QFileInfo::exists(model + QStringLiteral(".json"));
}

QStringList PiperVoiceCatalog::installedVoices()
{
    QStringList keys;
    const QFileInfoList files = QDir(PiperVoiceCatalog::voicesDir()).entryInfoList({QStringLiteral("*.onnx")}, QDir::Files, QDir::Name);

    for (const QFileInfo &file : files) {
        const QString key = file.completeBaseName();
        if (isInstalled(key))
            keys << key;
    }

    return keys;
}

QString PiperVoiceCatalog::modelFilePath(const QString &key)
{
    return PiperVoiceCatalog::voicesDir() + QLatin1Char('/') + key + QStringLiteral(".onnx");
}

void PiperVoiceCatalog::download(const QString &key)
{
    if (m_reply)
        return;

    m_current = voice(key);
    if (!m_current.isValid()) {
        emit downloadFinished(key, false, tr("Unknown voice: %1").arg(key));
        return;
    }

    QDir().mkpath(PiperVoiceCatalog::voicesDir());

    m_cancelled = false;
    m_doneBytes = 0;
    m_writtenFiles.clear();
    m_remainingFiles = QStringList{m_current.configPath, m_current.modelPath};

    emit logLine(QStringLiteral("Downloading %1 (%2 MB)")
                     .arg(m_current.key)
                     .arg(m_current.sizeBytes / 1024.0 / 1024.0, 0, 'f', 1));

    downloadNextFile();
}

void PiperVoiceCatalog::cancelDownload()
{
    if (!m_reply)
        return;

    m_cancelled = true;
    m_reply->abort();
}

void PiperVoiceCatalog::downloadNextFile()
{
    if (m_remainingFiles.isEmpty()) {
        finishDownload(true, {});
        return;
    }

    const QString repoPath = m_remainingFiles.takeFirst();
    const QString fileName = repoPath.section(QLatin1Char('/'), -1);
    const QString target = PiperVoiceCatalog::voicesDir() + QLatin1Char('/') + fileName;

    m_file = new QFile(target + QStringLiteral(".part"), this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        const QString error = m_file->errorString();
        delete m_file;
        m_file = nullptr;
        finishDownload(false, error);
        return;
    }

    emit logLine(fileName);

    m_reply = m_network->get(makeRequest(fileUrl(repoPath)));

    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
        emit downloadProgress(m_doneBytes + received, m_current.sizeBytes);
    });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, target] {
        const bool ok = m_reply->error() == QNetworkReply::NoError;
        const QString error = m_reply->errorString();

        if (ok)
            m_file->write(m_reply->readAll());

        m_doneBytes += m_file->size();
        m_file->close();

        const QString partPath = m_file->fileName();

        m_reply->deleteLater();
        m_reply = nullptr;
        m_file->deleteLater();
        m_file = nullptr;

        if (!ok) {
            QFile::remove(partPath);
            finishDownload(false, m_cancelled ? tr("Cancelled.") : error);
            return;
        }

        QFile::remove(target);
        if (!QFile::rename(partPath, target)) {
            QFile::remove(partPath);
            finishDownload(false, tr("Cannot save %1").arg(target));
            return;
        }

        m_writtenFiles.append(target);
        downloadNextFile();
    });
}

void PiperVoiceCatalog::finishDownload(bool ok, const QString &error)
{
    const QString key = m_current.key;
    m_current = {};
    m_remainingFiles.clear();

    if (!ok) {
        for (const QString &path : std::as_const(m_writtenFiles))
            QFile::remove(path);
    }
    m_writtenFiles.clear();

    if (ok)
        emit logLine(QStringLiteral("Voice ready: %1").arg(key));

    emit downloadFinished(key, ok, error);
}
