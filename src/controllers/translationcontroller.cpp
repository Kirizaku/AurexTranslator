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

#include "translationcontroller.h"

#include "src/translations/google.h"
#include "src/utils/ollama.h"

#include <QtNetwork/QNetworkAccessManager>

TranslationController::TranslationController(QNetworkAccessManager *manager, QObject *parent)
    : QObject(parent)
    , m_google(new Google(manager, this))
    , m_ollama(new Ollama(manager, this))
{
}

void TranslationController::setGoogleEnabled(bool enabled)
{
    m_googleEnabled = enabled;
}

void TranslationController::setOllamaEnabled(bool enabled)
{
    m_ollamaEnabled = enabled;
}

void TranslationController::setGoogleSourceLang(const QString &lang)
{
    m_google->setSourceLang(lang);
}

void TranslationController::setGoogleTargetLang(const QString &lang)
{
    m_google->setTargetLang(lang);
}

void TranslationController::setOllamaUrl(const QUrl &url)
{
    m_ollama->setUrl(url);
}

void TranslationController::setOllamaModel(const QString &model)
{
    m_ollamaModel = model;
}

void TranslationController::setOllamaPrompt(const QString &prompt)
{
    m_ollamaPrompt = prompt;
}

void TranslationController::translate(const QString &source, const QString &text)
{
    if (text.isEmpty()) return;

    if (m_googleEnabled) {
        m_google->translateText(text, [this, source, text](QString translated) {
            emit translationReady(source, QStringLiteral("Google"), text, translated);
        });
    }

    if (m_ollamaEnabled) {
        const QString prompt = m_ollamaPrompt + text;
        m_ollama->generate(prompt, m_ollamaModel, [this, source, text](QString translated) {
            emit translationReady(source, QStringLiteral("Ollama"), text, translated);
        });
    }
}