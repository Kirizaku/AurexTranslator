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

#ifndef TRANSLATIONCONTROLLER_H
#define TRANSLATIONCONTROLLER_H

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class Google;
class Ollama;

class TranslationController : public QObject
{
    Q_OBJECT

public:
    explicit TranslationController(QNetworkAccessManager *manager, QObject *parent = nullptr);

    // Translator enable/disable
    void setGoogleEnabled(bool enabled);
    void setOllamaEnabled(bool enabled);

    // Google settings
    void setGoogleSourceLang(const QString &lang);
    void setGoogleTargetLang(const QString &lang);

    // Ollama settings
    void setOllamaUrl(const QUrl &url);
    void setOllamaModel(const QString &model);
    void setOllamaPrompt(const QString &prompt);

public slots:
    void translate(const QString &source, const QString &text);

signals:
    void translationReady(const QString &source,
                          const QString &translatorName,
                          const QString &original,
                          const QString &translated);

private:
    Google *m_google = nullptr;
    bool m_googleEnabled = false;

    Ollama *m_ollama = nullptr;
    bool m_ollamaEnabled = false;
    QString m_ollamaModel;
    QString m_ollamaPrompt;
};

#endif // TRANSLATIONCONTROLLER_H