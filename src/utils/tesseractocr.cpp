/******************************************************************************
    Copyright (C) 2025 by Daniil Nabiulin

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
        emit currentStatus(QString(tr("<b>Inactive</b>")));
        m_tessApi->End();
    } else {
        emit currentStatus(QString(tr("<b>Active [%1]</b>")).arg(QString::fromUtf8(langPtr)));
        m_isRunning = true;
        start();
    }
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
        emit currentStatus(QString(tr("<b>Inactive</b>")));
    }
}

void TesseractOcr::frameMat(const cv::Mat &image)
{
    if (!image.empty()) {
        m_imageOcr = image.clone();
    }
}

void TesseractOcr::run()
{
    QString output;

    while (true) {
        QMutexLocker locker(&m_mutex);
        m_waitCondition.wait(&m_mutex, m_delay * 1000);

        if (!m_isRunning) break;

        if (!m_imageOcr.empty()) {
            m_tessApi->SetImage(m_imageOcr.data, m_imageOcr.cols, m_imageOcr.rows, 1, m_imageOcr.step[0]);
            output = m_tessApi->GetUTF8Text();

            if (cache_output != output) {
                cache_output = output;
                emit currentOutputOCR(output);
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

    if (availableLanguages.empty()) {
        Log(Logger::Level::Warning, "[tesseract] Failed to load language data or list is empty");
        return {};
    }
    return availableLanguages;
}
