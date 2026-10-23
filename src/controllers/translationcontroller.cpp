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
#include "src/engines/ollama.h"

#include <QtNetwork/QNetworkAccessManager>

TranslationController::TranslationController(QNetworkAccessManager *manager, QObject *parent)
    : QObject(parent)
    , m_google(new Google(manager, this))
    , m_ollama(new Ollama(manager, this))
{
    m_clock.start();

    m_googlePipeline.name = QStringLiteral("Google");
    m_googlePipeline.minIntervalMs = 1000;
    m_googlePipeline.pauseMs = 30000;
    m_googlePipeline.maxPauseMs = 30 * 60 * 1000;
    m_googlePipeline.cacheKey = [this](const QString &text) {
        return m_googleSourceLang + QLatin1Char('|') + m_googleTargetLang + QLatin1Char('|') + text.simplified();
    };
    m_googlePipeline.send = [this](const QString &text, std::function<void(TranslationResult)> done) {
        m_google->translateText(text, done);
    };

    m_ollamaPipeline.name = QStringLiteral("Ollama");
    m_ollamaPipeline.send = [this](const QString &text, std::function<void(TranslationResult)> done) {
        m_ollama->generate(m_ollamaPrompt + text, m_ollamaModel, [done](QString translated) {
            done({TranslationResult::Ok, translated});
        });
    };

    for (Pipeline *pipeline : {&m_googlePipeline, &m_ollamaPipeline}) {
        pipeline->timer.setSingleShot(true);
        connect(&pipeline->timer, &QTimer::timeout, this, [this, pipeline]() { sendNext(*pipeline); });
    }
}

void TranslationController::setGoogleEnabled(bool enabled)
{
    m_googleEnabled = enabled;
    if (!enabled) clearQueue(m_googlePipeline);
}

void TranslationController::setOllamaEnabled(bool enabled)
{
    m_ollamaEnabled = enabled;
    if (!enabled) clearQueue(m_ollamaPipeline);
}

void TranslationController::setGoogleSourceLang(const QString &lang)
{
    m_googleSourceLang = lang;
    m_google->setSourceLang(lang);
}

void TranslationController::setGoogleTargetLang(const QString &lang)
{
    m_googleTargetLang = lang;
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

    emit originalReady(source, text, m_googleEnabled || m_ollamaEnabled);

    if (m_googleEnabled) enqueue(m_googlePipeline, source, text);
    if (m_ollamaEnabled) enqueue(m_ollamaPipeline, source, text);
}

void TranslationController::enqueue(Pipeline &pipeline, const QString &source, const QString &text)
{
    if (!m_protectionEnabled) {
        dispatch(pipeline, source, text);
        return;
    }

    if (pipeline.cacheKey) {
        if (const QString *cached = pipeline.cache.object(pipeline.cacheKey(text))) {
            emit translationReady(source, pipeline.name, text, *cached);
            return;
        }
    }

    if (pipeline.pausedUntilMs > m_clock.elapsed()) {
        emit translationReady(source, pipeline.name, text, pauseMessage(pipeline));
    }

    if (!pipeline.pending.contains(source)) pipeline.order.append(source);
    pipeline.pending[source] = text;
    schedule(pipeline);
}

void TranslationController::schedule(Pipeline &pipeline)
{
    if (pipeline.order.isEmpty() || pipeline.timer.isActive()) return;

    const qint64 delay = std::max<qint64>(0, std::max(pipeline.nextSendMs, pipeline.pausedUntilMs) - m_clock.elapsed());
    pipeline.timer.start(static_cast<int>(delay));
}

void TranslationController::sendNext(Pipeline &pipeline)
{
    if (pipeline.order.isEmpty()) return;

    const QString source = pipeline.order.takeFirst();
    const QString text = pipeline.pending.take(source);
    pipeline.nextSendMs = m_clock.elapsed() + pipeline.minIntervalMs;

    dispatch(pipeline, source, text);
    schedule(pipeline);
}

void TranslationController::dispatch(Pipeline &pipeline, const QString &source, const QString &text)
{
    pipeline.send(text, [this, &pipeline, source, text](TranslationResult result) {
        finish(pipeline, source, text, result);
    });
}

void TranslationController::finish(Pipeline &pipeline, const QString &source, const QString &text, const TranslationResult &result)
{
    QString translated = result.text;

    switch (result.status) {
        case TranslationResult::Ok: {
            pipeline.failures = 0;
            if (pipeline.cacheKey) pipeline.cache.insert(pipeline.cacheKey(text), new QString(result.text));
            break;
        }
        case TranslationResult::RateLimited: {
            if (m_protectionEnabled) {
                const qint64 pauseMs = std::min<qint64>(pipeline.maxPauseMs, static_cast<qint64>(pipeline.pauseMs) << std::min(pipeline.failures, 6));
                ++pipeline.failures;
                pipeline.pausedUntilMs = m_clock.elapsed() + pauseMs;
                translated = pauseMessage(pipeline);
            } else {
                translated = tr("Translation failed: %1 is limiting requests").arg(pipeline.name);
            }
            break;
        }
        case TranslationResult::Error: {
            translated = tr("Translation failed: %1").arg(result.text);
            break;
        }
    }

    emit translationReady(source, pipeline.name, text, translated);
}

void TranslationController::clearQueue(Pipeline &pipeline)
{
    pipeline.pending.clear();
    pipeline.order.clear();
    pipeline.timer.stop();
    pipeline.pausedUntilMs = 0;
    pipeline.failures = 0;
}

QString TranslationController::pauseMessage(const Pipeline &pipeline) const
{
    const int seconds = static_cast<int>(std::max<qint64>(0, pipeline.pausedUntilMs - m_clock.elapsed() + 999) / 1000);
    const QString wait = seconds < 60 ? tr("%1 s").arg(seconds) : tr("%1 min").arg((seconds + 59) / 60);
    return tr("Translation paused: %1 is limiting requests, retrying in %2").arg(pipeline.name, wait);
}
