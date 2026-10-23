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
#include "src/utils/logger.h"

namespace {

// A line whose words average this far above the word threshold is kept
constexpr float kLineConfMargin = 20.0f;
constexpr float kMaxLineConf = 95.0f;
// Stable: fraction of pixels changed since the previous tick must stay below this
constexpr double kStableMaxChangedRatio = 0.01;
// Changed: fraction of pixels changed since the last OCR'd frame must reach this;
// 0.02 at sensitivity 0, 0.002 at 50, 0.0002 at 100
constexpr double kChangedRatioAtZero = 0.02;
// Per-pixel difference (0-255) that counts as a real change, not noise
constexpr int kPixelDiffThreshold = 25;
// Noisy source (e.g. video): stop waiting for stability after this many ticks
constexpr int kMaxUnstableTicks = 5;
// Texts closer than 10% of their length (capped by the user setting) are treated as the same
constexpr int kTextDistanceDivisor = 10;

cv::Mat makeThumb(const cv::Mat &image)
{
    cv::Mat thumb;
    cv::resize(image, thumb, cv::Size(), 0.25, 0.25, cv::INTER_AREA);
    return thumb;
}

double changedRatio(const cv::Mat &a, const cv::Mat &b)
{
    if (a.empty() || b.empty() || a.size() != b.size() || a.type() != b.type()) return 1.0;

    cv::Mat diff;
    cv::absdiff(a, b, diff);
    if (diff.channels() > 1) cv::cvtColor(diff, diff, cv::COLOR_BGR2GRAY);
    return static_cast<double>(cv::countNonZero(diff > kPixelDiffThreshold)) / diff.total();
}

int levenshtein(const QString &a, const QString &b)
{
    QVector<int> prev(b.size() + 1), curr(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j) prev[j] = j;

    for (int i = 1; i <= a.size(); ++i) {
        curr[0] = i;
        for (int j = 1; j <= b.size(); ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}

// CJK text has no spaces between words, but Tesseract still returns it split into pieces
// ("これ" "は" "日本" "語"). Joining those with spaces would give "これ は 日本 語", so
// characters of this kind are glued together. Covers Han/kana/Hangul, the CJK punctuation
// block ("。", "「", "ー") and fullwidth forms ("！", "？")
bool isCjkOrFullwidth(QChar c)
{
    const char16_t u = c.unicode();
    if ((u >= 0x3000 && u <= 0x30FF) || (u >= 0xFF00 && u <= 0xFFEF)) return true;

    switch (QChar::script(u)) {
    case QChar::Script_Han:
    case QChar::Script_Hiragana:
    case QChar::Script_Katakana:
    case QChar::Script_Hangul:
        return true;
    default:
        return false;
    }
}

tesseract::PageSegMode choosePsm(int setting, int width, int height)
{
    switch (setting) {
    case TesseractTuning::PsmBlock: return tesseract::PSM_SINGLE_BLOCK;
    case TesseractTuning::PsmLine: return tesseract::PSM_SINGLE_LINE;
    case TesseractTuning::PsmSparse: return tesseract::PSM_SPARSE_TEXT;
    default: break;
    }

    const double aspect = static_cast<double>(width) / std::max(1, height);

    if (width <= 72 && height <= 72 && aspect < 2.2) return tesseract::PSM_SINGLE_WORD;
    if ((height <= 40 && aspect >= 2.0) || (height <= 56 && aspect >= 5.0)) return tesseract::PSM_SINGLE_LINE;
    if (width >= 900 && height >= 500) return tesseract::PSM_SPARSE_TEXT;
    return tesseract::PSM_SINGLE_BLOCK;
}

bool joinsWithoutSpace(QChar left, QChar right)
{
    return (isCjkOrFullwidth(left) && (isCjkOrFullwidth(right) || right.isPunct()))
        || (isCjkOrFullwidth(right) && left.isPunct());
}

QString joinWords(const QStringList &words)
{
    QString line;
    for (const QString &word : words) {
        if (!line.isEmpty() && !joinsWithoutSpace(line.back(), word.front())) line += QLatin1Char(' ');
        line += word;
    }
    return line;
}

}

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
    if (image.empty()) return;

    cv::Mat copy = image.clone();
    QMutexLocker locker(&m_mutex);
    m_imageOcr = copy;
}

void TesseractOcr::triggerManualOCR()
{
    QMutexLocker locker(&m_mutex);
    if (m_mode == Mode::Manual) {
        m_requestOCR = true;
        m_waitCondition.wakeOne();
    }
}

void TesseractOcr::setMode(const int id)
{
    QMutexLocker locker(&m_mutex);
    m_mode = static_cast<Mode>(id);
    if (m_mode == Auto) {
        m_requestOCR = true;
        m_waitCondition.wakeOne();
    }
}

void TesseractOcr::setDelay(const double &value)
{
    QMutexLocker locker(&m_mutex);
    m_delay = value;
    m_waitCondition.wakeOne();
}

void TesseractOcr::clearCache()
{
    QMutexLocker locker(&m_mutex);
    m_resetGatesPending = true;
}

void TesseractOcr::setTuning(const TesseractTuning &tuning)
{
    QMutexLocker locker(&m_mutex);
    if (m_tuning == tuning) return;

    m_tuning = tuning;
    m_resetGatesPending = true;
    m_resetStatsPending = true;
    m_waitCondition.wakeOne();
}

void TesseractOcr::resetStats()
{
    QMutexLocker locker(&m_mutex);
    m_resetStatsPending = true;
}

void TesseractOcr::resetGates()
{
    m_lastEmitted.clear();
    m_hasEmitted = false;
    m_lastOcrThumb.release();
    m_prevTickThumb.release();
    m_unstableTicks = 0;
}

void TesseractOcr::clearStats()
{
    m_statRuns = m_statSkippedUnchanged = m_statDuplicates = m_statSent = 0;
}

bool TesseractOcr::imageGatePassed(const cv::Mat &thumb, const TesseractTuning &tuning)
{
    bool stable = changedRatio(thumb, m_prevTickThumb) < kStableMaxChangedRatio;
    m_prevTickThumb = thumb;

    if (!stable && ++m_unstableTicks < kMaxUnstableTicks) return false;
    m_unstableTicks = 0;

    const double minRatio = kChangedRatioAtZero * std::pow(0.1, tuning.changeSensitivity / 50.0);
    return changedRatio(thumb, m_lastOcrThumb) >= minRatio;
}

bool TesseractOcr::textGatePassed(const QString &text, const TesseractTuning &tuning)
{
    if (!m_hasEmitted) return !text.isEmpty();

    int allowed = 0;
    if (tuning.skipUnchanged) {
        allowed = std::min(tuning.textTolerance, static_cast<int>(m_lastEmitted.size()) / kTextDistanceDivisor);
    }
    if (std::abs(text.size() - m_lastEmitted.size()) > allowed) return true;

    return levenshtein(text, m_lastEmitted) > allowed;
}

QString TesseractOcr::recognize(const cv::Mat &image, const TesseractTuning &tuning, QString *dropped)
{
    m_tessApi->SetPageSegMode(choosePsm(tuning.psm, image.cols, image.rows));
    m_tessApi->SetImage(image.data, image.cols, image.rows, 1, image.step[0]);
    if (m_tessApi->Recognize(nullptr) != 0) return QString();

    std::unique_ptr<tesseract::ResultIterator> it(m_tessApi->GetIterator());
    if (!it) return QString();

    const bool filter = tuning.filterNoise;
    const float minWordConf = filter ? static_cast<float>(tuning.minConfidence) : 0.0f;
    const float minLineConf = filter ? std::min(kMaxLineConf, minWordConf + kLineConfMargin) : 0.0f;

    struct Word { QString text; float conf; };
    QStringList lines, droppedLines, droppedWords;
    std::vector<Word> words;

    auto flushLine = [&]() {
        int first = 0, last = static_cast<int>(words.size()) - 1;
        while (first <= last && words[first].conf < minWordConf) droppedWords << words[first++].text;
        while (last >= first && words[last].conf < minWordConf) droppedWords << words[last--].text;

        QStringList kept;
        float confSum = 0;
        for (int i = first; i <= last; ++i) { kept << words[i].text; confSum += words[i].conf; }
        if (!kept.isEmpty()) {
            const QString line = joinWords(kept);
            const bool hasText = std::any_of(line.begin(), line.end(), [](QChar ch) { return ch.isLetterOrNumber(); });
            const bool keep = confSum / kept.size() >= minLineConf && (hasText || !filter);
            (keep ? lines : droppedLines) << line;
        }
        words.clear();
    };

    do {
        if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) flushLine();
        const float conf = it->Confidence(tesseract::RIL_WORD);
        char *raw = it->GetUTF8Text(tesseract::RIL_WORD);
        const QString word = QString::fromUtf8(raw).trimmed();
        delete[] raw;
        if (!word.isEmpty()) words.push_back({word, conf});
    } while (it->Next(tesseract::RIL_WORD));
    flushLine();

    if (dropped) {
        QStringList parts = droppedLines;
        if (!droppedWords.isEmpty()) parts << tr("(words: %1)").arg(droppedWords.join(QLatin1String(", ")));
        *dropped = parts.join(QLatin1Char('\n'));
    }
    return lines.join(QLatin1Char('\n'));
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
    while (true) {
        cv::Mat image;
        TesseractTuning tuning;
        Mode mode;

        {
            QMutexLocker locker(&m_mutex);

            if (m_mode == Mode::Auto) {
                m_waitCondition.wait(&m_mutex, m_delay * 1000);
            } else if (m_mode == Mode::Manual) {
                while (m_isRunning && !m_requestOCR) {
                    m_waitCondition.wait(&m_mutex);
                }
                m_requestOCR = false;
            }

            if (!m_isRunning) break;

            if (m_resetGatesPending) resetGates();
            if (m_resetStatsPending) clearStats();
            m_resetGatesPending = m_resetStatsPending = false;

            image = m_imageOcr;
            tuning = m_tuning;
            mode = m_mode;
        }

        if (image.empty()) continue;

        if (mode == Mode::Auto && tuning.skipUnchanged) {
            const cv::Mat thumb = makeThumb(image);
            if (!imageGatePassed(thumb, tuning)) {
                ++m_statSkippedUnchanged;
                emit statsUpdated(m_statRuns, m_statSkippedUnchanged, m_statDuplicates, m_statSent);
                continue;
            }
            m_lastOcrThumb = thumb;
        }

        QString dropped;
        const QString output = recognize(image, tuning, &dropped);
        ++m_statRuns;
        emit recognized(output.trimmed(), dropped);

        const QString key = output.simplified();
        if (textGatePassed(key, tuning)) {
            m_lastEmitted = key;
            m_hasEmitted = true;
            ++m_statSent;
            emit currentOutputOCR("Tesseract", output.trimmed());
        } else {
            ++m_statDuplicates;
        }
        emit statsUpdated(m_statRuns, m_statSkippedUnchanged, m_statDuplicates, m_statSent);
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
