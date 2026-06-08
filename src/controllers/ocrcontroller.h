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

#ifndef OCRCONTROLLER_H
#define OCRCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <opencv2/core/mat.hpp>

class QNetworkAccessManager;
class QTimer;
class TesseractOcr;
class Ollama;

class OcrController : public QObject
{
    Q_OBJECT

public:
    enum Engine { Tesseract, OllamaVision };

    explicit OcrController(QNetworkAccessManager *manager, QObject *parent = nullptr);
    ~OcrController();

    // Active engine selection
    void setEngine(Engine engine);
    Engine engine() const { return m_engine; }

    // Enable/disable the whole OCR pipeline
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void applyConfiguration();

    // Tesseract settings
    void setTesseractTessdataPath(const QString &path);
    void setTesseractUseSystemTessdata(bool useSystem);
    void setTesseractLanguage(const QString &lang);
    void setTesseractMode(int mode);
    void setTesseractAutoInterval(double seconds);
    QStringList availableTesseractLanguages();
    bool isTesseractRunning() const;
    QString tesseractActiveLang() const { return m_tesseractActiveLang; }

    // Ollama Vision settings
    void setOllamaUrl(const QUrl &url);
    void setOllamaModel(const QString &model);
    void setOllamaVisionPrompt(const QString &prompt);
    void setOllamaVisionMode(int mode);    // 0 = Auto, 1 = Manual
    void setOllamaVisionAutoInterval(int seconds);
    void setOllamaWaitForResponse(bool wait);

    Ollama* ollama() const { return m_ollama; }
    TesseractOcr* tesseract() const { return m_tesseractOcr; }

public slots:
    // OpenCV calls this with processed Mat
    void processFrame(const cv::Mat &frame);

    // Manual trigger ("retranslate" or "manual translate")
    void triggerManual();

signals:
    // OCR produced text. Source string is "Tesseract" or "Ollama Vision"
    void textRecognized(const QString &source, const QString &text);

    // An engine was deactivated and its previous UI results should be cleared
    void engineDeactivated(const QString &source);

private slots:
    void onOllamaVisionTimerTimeout();
    void onTesseractOutput(const QString &source, const QString &text);

private:
    Engine m_engine = Tesseract;
    bool m_enabled = false;

    TesseractOcr *m_tesseractOcr = nullptr;
    Ollama *m_ollama = nullptr;
    QTimer *m_ollamaVisionTimer = nullptr;

    // Tesseract config
    QString m_tesseractActiveLang;
    QString m_tesseractTessdataPath;
    bool m_tesseractUseSystemTessdata = true;
    int m_tesseractMode = 0;
    double m_tesseractAutoInterval = 1.0;
    QStringList m_availableLanguages;

    // Ollama Vision config
    QString m_ollamaVisionPrompt;
    QString m_ollamaCurrentModel;
    int m_ollamaVisionMode = 0;
    int m_ollamaVisionAutoInterval = 3;
    bool m_ollamaWaitForResponse = true;
    bool m_ollamaVisionRequestInProgress = false;
    QString m_ollamaVisionCacheOutput;

    void stopAllEngines();
    void stopTesseract();
    void stopOllamaVision();
    void configureTesseract();
    void configureOllamaVision();
    void doOllamaVisionRequest();
};

#endif // OCRCONTROLLER_H