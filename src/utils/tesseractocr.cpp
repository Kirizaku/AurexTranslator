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

#include "tesseractocr.h"
#include "logger.h"

TesseractOcr::TesseractOcr(QObject *parent)
    : QThread{parent}
{
    m_tessApi = new tesseract::TessBaseAPI();
    m_tessApi->SetPageSegMode(tesseract::PSM_AUTO);
#if TESSERACT_MAJOR_VERSION < 5
    #ifdef Q_OS_LINUX
        m_tessApi->SetVariable("debug_file", "/dev/null");
    #else
        m_tessApi->SetVariable("debug_file", "nul");
    #endif
#endif
}

TesseractOcr::~TesseractOcr()
{
    delete m_tessApi;
    m_tessApi = nullptr;
}

void TesseractOcr::init(const QString &lang)
{
    QByteArray langBytes = lang.toUtf8();
    const char* langPtr = lang.isEmpty() ? nullptr : langBytes.constData();

    if (m_tessApi->Init(m_tessdataPath.toUtf8().constData(), langPtr)) {
        Log(Logger::Level::Warning, "Failed to initialize Tesseract");
        m_tessApi->End();
        return;
    }

    if (lang.isEmpty()) {
        m_tessApi->End();
    } else {
        m_currentLang = lang;
        m_isRunning = true;
        start();
    }
}

void TesseractOcr::frameMat(const cv::Mat &image)
{
    if (!image.empty()) {
        m_imageOcr = image.clone();
    }
}

void TesseractOcr::triggerManualOCR()
{
    if (m_mode == Mode::Manual) {
        requestOCR();
    }
}

void TesseractOcr::setMode(const int id)
{
    m_mode = static_cast<Mode>(id);
    if (m_mode == Auto) {
        m_requestOCR = true;
        m_waitCondition.wakeOne();
    }
}

void TesseractOcr::requestOCR()
{
    QMutexLocker locker(&m_mutex);
    m_requestOCR = true;
    m_waitCondition.wakeOne();
}

void TesseractOcr::stop()
{
    if (m_tessApi) {
        {
            QMutexLocker locker(&m_mutex);
            m_isRunning = false;
            m_waitCondition.wakeAll();
        }
        wait();
        m_tessApi->End();
    }
}

void TesseractOcr::run()
{
    QString output;

    while (true) {
        QMutexLocker locker(&m_mutex);

        if (m_mode == Mode::Auto) {
            m_waitCondition.wait(&m_mutex, m_delay * 1000);
        } else if (m_mode == Mode::Manual) {
            while (m_isRunning && !m_requestOCR) {
                m_waitCondition.wait(&m_mutex);
            }
            if (m_requestOCR) {
                m_requestOCR = false;
            }
        }

        if (!m_isRunning) break;

        if (!m_imageOcr.empty()) {
            m_tessApi->SetImage(m_imageOcr.data, m_imageOcr.cols, m_imageOcr.rows, 1, m_imageOcr.step[0]);
            output = m_tessApi->GetUTF8Text();

            if (cache_output != output) {
                cache_output = output;
                emit currentOutputOCR("Tesseract", output.trimmed());
            }
        }
    }
}

std::vector<std::string> TesseractOcr::checkAvailableLanguages()
{
    if (m_tessApi->Init(m_tessdataPath.toUtf8().constData(), NULL)) {
        Log(Logger::Level::Warning, "Failed to initialize Tesseract");
        m_tessApi->End();
        return {};
    }

#if TESSERACT_MAJOR_VERSION < 5
        GenericVector<STRING> tessLanguages;
        m_tessApi->GetAvailableLanguagesAsVector(&tessLanguages);

        std::vector<std::string> availableLanguages;
        for (int i = 0; i < tessLanguages.size(); ++i) {
            availableLanguages.emplace_back(tessLanguages[i].string());
        }
#else
        std::vector<std::string> availableLanguages;
        m_tessApi->GetAvailableLanguagesAsVector(&availableLanguages);
#endif
        m_tessApi->End();

    if (availableLanguages.empty()) {
        Log(Logger::Level::Warning, "[tesseract] Failed to load language data or list is empty");
        return {};
    }
    return availableLanguages;
}
