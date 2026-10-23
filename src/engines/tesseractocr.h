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

struct TesseractTuning
{
    enum Psm {
        PsmAuto,
        PsmBlock,
        PsmLine,
        PsmSparse
    };

    bool skipUnchanged = true;
    bool filterNoise = true;

    // 0-100: how much of the picture has to change before it is read again
    int changeSensitivity = 50;
    // 0-10: texts differing by at most this many characters count as the same
    int textTolerance = 3;
    // 0-80: words below this confidence are dropped as noise
    int minConfidence = 45;
    int psm = PsmAuto;

    bool operator==(const TesseractTuning &other) const
    {
        return skipUnchanged == other.skipUnchanged && filterNoise == other.filterNoise
            && changeSensitivity == other.changeSensitivity && textTolerance == other.textTolerance
            && minConfidence == other.minConfidence && psm == other.psm;
    }
};

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
    QString getTessdataPath() { return m_tessdataPath; }
    QString getLanguage() { return m_currentLang; }
    void setMode(const int id);
    void setDelay(const double &value);
    void clearCache();
    void setTuning(const TesseractTuning &tuning);
    void resetStats();

public slots:
    void frameMat(const cv::Mat &image);
    void triggerManualOCR();

signals:
    void currentOutputOCR(const QString &source, const QString &output);
    void statsUpdated(int runs, int skippedUnchanged, int duplicates, int sent);
    void recognized(const QString &text, const QString &dropped);

protected:
    void run() override;

private:
    void resetGates();
    void clearStats();
    bool imageGatePassed(const cv::Mat &thumb, const TesseractTuning &tuning);
    bool textGatePassed(const QString &text, const TesseractTuning &tuning);
    QString recognize(const cv::Mat &image, const TesseractTuning &tuning, QString *dropped);
    enum Mode {
        Auto,
        Manual
    };

    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    tesseract::TessBaseAPI *m_tessApi = nullptr;
    QString m_tessdataPath = "";
    QString m_currentLang = "";

    // Guarded by m_mutex
    bool m_isRunning = false;
    double m_delay = 1;
    cv::Mat m_imageOcr;
    TesseractTuning m_tuning;
    Mode m_mode = Mode::Auto;
    bool m_requestOCR = false;
    bool m_resetGatesPending = false;
    bool m_resetStatsPending = false;

    // Worker thread only
    cv::Mat m_prevTickThumb;
    cv::Mat m_lastOcrThumb;
    int m_unstableTicks = 0;
    QString m_lastEmitted;
    bool m_hasEmitted = false;
    int m_statRuns = 0;
    int m_statSkippedUnchanged = 0;
    int m_statDuplicates = 0;
    int m_statSent = 0;
};

#endif // OCR_H
