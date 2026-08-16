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

#ifndef PIPERVOICECATALOG_H
#define PIPERVOICECATALOG_H

#include <QObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

struct PiperVoice {
    QString key;
    QString name;
    QString quality;
    QString languageCode;
    QString languageName;
    int numSpeakers = 1;
    qint64 sizeBytes = 0;
    QString modelPath;
    QString configPath;

    bool isValid() const { return !key.isEmpty() && !modelPath.isEmpty(); }
};

class PiperVoiceCatalog : public QObject
{
    Q_OBJECT

public:
    explicit PiperVoiceCatalog(QObject *parent = nullptr);

    static QString voicesDir();
    static QString defaultRepoBase();
    static QString repoBase();
    static QString configuredRepoBase();
    static void setRepoBase(const QString &base);

    static QString voicesJsonUrl();
    static QString fileUrl(const QString &repoPath);

    void refresh();
    bool isLoaded() const { return !m_voices.isEmpty(); }
    bool refreshing() const { return m_refreshReply != nullptr; }

    QList<PiperVoice> voices() const { return m_voices; }
    QStringList languages() const;
    PiperVoice voice(const QString &key) const;

    static bool isInstalled(const QString &key);
    static QStringList installedVoices();
    static QString modelFilePath(const QString &key);

    void download(const QString &key);
    void cancelDownload();
    bool downloading() const { return m_reply != nullptr; }

signals:
    void refreshed(bool ok, const QString &error);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &key, bool ok, const QString &error);
    void logLine(const QString &line);

private:
    void downloadNextFile();
    void finishDownload(bool ok, const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QList<PiperVoice> m_voices;
    QNetworkReply *m_refreshReply = nullptr;

    // Current download
    PiperVoice m_current;
    QStringList m_remainingFiles;
    QStringList m_writtenFiles;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    qint64 m_doneBytes = 0;
    bool m_cancelled = false;
};

#endif // PIPERVOICECATALOG_H
