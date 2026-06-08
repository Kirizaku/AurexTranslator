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

#include "ocrcontroller.h"

#include "src/utils/tesseractocr.h"
#include "src/utils/ollama.h"
#include "src/utils/logger.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QTimer>
#include <QDir>

OcrController::OcrController(QNetworkAccessManager *manager, QObject *parent)
    : QObject(parent)
    , m_tesseractOcr(new TesseractOcr(this))
    , m_ollama(new Ollama(manager, this))
{
    connect(m_tesseractOcr, &TesseractOcr::currentOutputOCR,
            this, &OcrController::onTesseractOutput);
}

OcrController::~OcrController()
{
    if (m_tesseractOcr && m_tesseractOcr->isRunning()) {
        m_tesseractOcr->stop();
        m_tesseractOcr->wait();
    }
    if (m_ollamaVisionTimer) {
        m_ollamaVisionTimer->stop();
    }
}

void OcrController::setEngine(Engine engine)
{
    m_engine = engine;
}

void OcrController::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (!m_enabled) {
        stopAllEngines();
        emit engineDeactivated(QStringLiteral("Tesseract"));
        emit engineDeactivated(QStringLiteral("Ollama Vision"));
    }
}

void OcrController::applyConfiguration()
{
    if (!m_enabled) {
        stopAllEngines();
        return;
    }

    if (m_engine == Tesseract) {
        stopOllamaVision();
        configureTesseract();
    } else {
        stopTesseract();
        configureOllamaVision();
    }
}

// ===============================================================
// Tesseract settings
// ===============================================================

void OcrController::setTesseractTessdataPath(const QString &path)
{
    m_tesseractTessdataPath = path;
}

void OcrController::setTesseractUseSystemTessdata(bool useSystem)
{
    m_tesseractUseSystemTessdata = useSystem;
}

void OcrController::setTesseractLanguage(const QString &lang)
{
    m_tesseractActiveLang = lang;
}

void OcrController::setTesseractMode(int mode)
{
    m_tesseractMode = mode;
    if (m_tesseractOcr) m_tesseractOcr->setMode(mode);
}

void OcrController::setTesseractAutoInterval(double seconds)
{
    m_tesseractAutoInterval = seconds;
    if (m_tesseractOcr) m_tesseractOcr->setDelay(seconds);
}

QStringList OcrController::availableTesseractLanguages()
{
    return m_availableLanguages;
}

bool OcrController::isTesseractRunning() const
{
    return m_tesseractOcr && m_tesseractOcr->isRunning();
}

// ===============================================================
// Ollama settings
// ===============================================================

void OcrController::setOllamaUrl(const QUrl &url)
{
    if (m_ollama) m_ollama->setUrl(url);
}

void OcrController::setOllamaModel(const QString &model)
{
    m_ollamaCurrentModel = model;
}

void OcrController::setOllamaVisionPrompt(const QString &prompt)
{
    m_ollamaVisionPrompt = prompt;
}

void OcrController::setOllamaVisionMode(int mode)
{
    m_ollamaVisionMode = mode;
}

void OcrController::setOllamaVisionAutoInterval(int seconds)
{
    m_ollamaVisionAutoInterval = seconds;
    if (m_ollamaVisionTimer) {
        m_ollamaVisionTimer->setInterval(seconds * 1000);
    }
}

void OcrController::setOllamaWaitForResponse(bool wait)
{
    m_ollamaWaitForResponse = wait;
}

// ===============================================================
// Slots
// ===============================================================

void OcrController::processFrame(const cv::Mat &frame)
{
    if (!m_enabled || frame.empty()) return;

    if (m_engine == Tesseract) {
        if (m_tesseractOcr) m_tesseractOcr->frameMat(frame);
    } else {
        if (m_ollama) m_ollama->frameMat(frame);
    }
}

void OcrController::triggerManual()
{
    if (!m_enabled) return;

    if (m_engine == Tesseract) {
        if (m_tesseractOcr) {
            m_tesseractOcr->clearCache();
            m_tesseractOcr->triggerManualOCR();
        }
    } else {
        if (m_ollama) {
            m_ollama->generateVision(m_ollamaVisionPrompt, m_ollamaCurrentModel,
                                     [this](QString result) {
                                         emit textRecognized(QStringLiteral("Ollama Vision"), result);
                                     });
        }
    }
}

void OcrController::onOllamaVisionTimerTimeout()
{
    doOllamaVisionRequest();
}

void OcrController::onTesseractOutput(const QString &source, const QString &text)
{
    emit textRecognized(source, text);
}

// ===============================================================
// Engines
// ===============================================================

void OcrController::stopAllEngines()
{
    stopTesseract();
    stopOllamaVision();
}

void OcrController::stopTesseract()
{
    if (m_tesseractOcr && m_tesseractOcr->isRunning()) {
        m_tesseractOcr->stop();
    }
}

void OcrController::stopOllamaVision()
{
    if (m_ollamaVisionTimer) {
        m_ollamaVisionTimer->stop();
        delete m_ollamaVisionTimer;
        m_ollamaVisionTimer = nullptr;
    }
    m_ollamaVisionCacheOutput.clear();
    m_ollamaVisionRequestInProgress = false;
}

void OcrController::configureTesseract()
{
    stopTesseract();

    emit engineDeactivated(QStringLiteral("Ollama Vision"));

    if (m_tesseractUseSystemTessdata) {
        m_tesseractOcr->setTessdataPath(QString());
    } else {
        QDir dir(m_tesseractTessdataPath);
        if (dir.exists()) {
            m_tesseractOcr->setTessdataPath(m_tesseractTessdataPath);
        } else {
            m_tesseractUseSystemTessdata = true;
            Log(Logger::Level::Warning,
                "[tesseract] The specified Tesseract data directory does not exist or is invalid");
        }
    }

    // Collect available languages FIRST. checkAvailableLanguages() does Init(path, NULL)
    // + End() internally, which must happen BEFORE the real init(lang) starts the worker
    // thread - otherwise the second Init clobbers the running TessAPI state and
    // GetUTF8Text() starts returning empty strings
    m_availableLanguages.clear();
    auto languages = m_tesseractOcr->checkAvailableLanguages();
    for (const auto& language : languages) {
        m_availableLanguages << QString::fromStdString(language);
    }

    if (!m_tesseractActiveLang.isEmpty()) {
        m_tesseractOcr->init(m_tesseractActiveLang);
    }
}

void OcrController::configureOllamaVision()
{
    emit engineDeactivated(QStringLiteral("Tesseract"));

    if (m_ollamaVisionMode == 0 /* Auto */) {
        if (!m_ollamaVisionTimer) {
            m_ollamaVisionTimer = new QTimer(this);
            connect(m_ollamaVisionTimer, &QTimer::timeout,
                    this, &OcrController::onOllamaVisionTimerTimeout);
        }
        m_ollamaVisionTimer->setInterval(m_ollamaVisionAutoInterval * 1000);
        m_ollamaVisionTimer->start();
    } else {
        stopOllamaVision();
    }
}

void OcrController::doOllamaVisionRequest()
{
    if (m_ollamaWaitForResponse && m_ollamaVisionRequestInProgress) {
        return;
    }

    m_ollamaVisionRequestInProgress = true;

    m_ollama->generateVision(m_ollamaVisionPrompt, m_ollamaCurrentModel,
                             [this](QString result) {
                                 m_ollamaVisionRequestInProgress = false;

                                 if (m_ollamaVisionCacheOutput == result || result.isEmpty()) {
                                     return;
                                 }
                                 m_ollamaVisionCacheOutput = result;
                                 emit textRecognized(QStringLiteral("Ollama Vision"), result);
                             });
}