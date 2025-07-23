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

#ifndef OCR_H
#define OCR_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <tesseract/baseapi.h>
#if TESSERACT_MAJOR_VERSION < 5
#include <tesseract/genericvector.h>
#endif
#include <opencv2/opencv.hpp>

class TesseractOcr : public QThread
{
    Q_OBJECT

public:
    explicit TesseractOcr(QObject *parent = nullptr);
    ~TesseractOcr();
    void init(const QString &lang);
    void stop();
    std::vector<std::string> checkAvailableLanguages();
    void setTessdataPath(const QString &value) { m_tessdataPath = value; }
    void setDelay(const double &value) { m_delay = value; }
    void clearCache() { cache_output.clear(); }

public slots:
    void frameMat(const cv::Mat &image);

signals:
    void currentStatus(const QString &status);
    void currentOutputOCR(const QString &output);

protected:
    void run() override;

private:
    bool m_isRunning = false;
    QString m_tessdataPath = "";
    double m_delay = 1;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    tesseract::TessBaseAPI *m_tessApi = nullptr;
    cv::Mat m_imageOcr;
    QString cache_output = "";
};

#endif // OCR_H
