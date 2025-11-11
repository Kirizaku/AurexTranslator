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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

#ifdef __linux__
#include <globalshortcuts_portal_interface.h>
#include "src/utils/portal_hotkeys.h"
#include "src/screencasts/linux-capture-portal/screencast-pipewire.h"
#include "src/screencasts/linux-capture-portal/screencast-portal.h"
#include "src/screencasts/linux-capture-x11/screencast-x11.h"
#endif

#include "src/utils/opencv.h"
#include "src/utils/tesseractocr.h"
#include "src/utils/ollama.h"
#include "src/utils/hotkeys.h"
#include "src/translations/google.h"
#include "tesseractsettingsdialog.h"
#include "ollamasettingsdialog.h"
#include "textoutputwindow.h"
#include "screencastwindow.h"
#include "overlaywindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void currentOverlayText(const QString &translatorName, const QString &original, const QString &result);
    void clearOverlayText(const QString &translatorName);
    void on_showHistoryText();

private slots:
    void on_availableGeometryChanged();
    void on_widgetChanged();
    void on_translatorChanged();
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_portalShortcutActivated(const QString &shortcutId);
    void on_portalShortcutDeactivated();

#ifdef __linux__
    void on_generalBindShortcut_clicked();
#endif
    void on_outputGeneralSelect_clicked();
    void on_outputProcessedOtsu_stateChanged(int arg1);
    void on_translatorOnlineGoogleSettingsButton_clicked();
    void on_textProcessingOCREngineTesseractSettingsButton_clicked();
    void on_logsNewLogMessage(const QString& message);
    void on_logsCopyAllButton_clicked();
    void on_logsOpenDirectoryButton_clicked();
    void on_proxyEnabledCheckBox_stateChanged(int arg1);

    // Pipewire
    void setCurrentRestoreToken(const QString &restoreToken);
    void setCurrentNodeId(const uint &nodeId);

    // OpenCV
    void setCurrentOriginalFrame(const QImage &frame);
    void setCurrentProcessedFrame(const QImage &frame);
    void setCurrentProcessedMat(const cv::Mat &frame);

    // OCR
    void setCurrentOutputOCR(const QString &output);
    void on_outputToggledOriginalScreencast_stateChanged(int arg1);
    void on_outputToggledProcessedScreencast_stateChanged(int arg1);
    void on_textProcessingAddRowButton_clicked();
    void on_textProcessingRemoveRowButton_clicked();
    void openOllamaSettings();
    void ollamaVisionTimerTimeout();
    void triggerManualOCR();

    // Output Window
    void selectNewRegionRequest();
    void selectNewInnerRegionRequest();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *m_manager;
    QScreen *m_screen;
    TextOutputWindow *m_outputWindow = nullptr;

    // Global Shortcuts
    HotKeys *m_captureRegionHotKey = nullptr;
    HotKeys *m_showHistoryTranslationHotKey = nullptr;
    HotKeys *m_manualTranslateHotKey = nullptr;
#ifdef __linux__
    PortalHotkeys *m_portalHotKeys = nullptr;
#endif
    bool m_isShortcuts = false;
    void initHotKeys();

    // Overlay
    OverlayWindow *m_overlayWindow = nullptr;
    QImage m_overlayImage;
    void captureRegion();
    void showHistory();
    void showOverlayWindow();

    // Screen Casting
#ifdef __linux__
    Pipewire *m_pipewire = nullptr;
    bool m_stopPipewire = false;
    ScreenCastPortal *m_portalScreencast = nullptr;
#endif
    ScreenCast *m_screenCapture = nullptr;
    ScreenCastWindow *m_screenCastWindow = nullptr;
    OpenCV *m_opencv = nullptr;
    void initScreenCast();

    // Translator
    Ollama *m_ollama = nullptr;
    OllamaSettingsDialog *m_ollamaSettingsDialog = nullptr;
    QString m_ollamaUrl = "http://localhost:11434/";
    QString m_ollamaCurrentModel;
    QString m_ollamaTranslationPrompt = "";
    QString m_ollamaVisionPrompt = "";
    QJsonArray m_ollamaModels;

    Google *m_google = nullptr;
    QString m_googleSourceLang;
    QString m_googleTargetLang;

    // OCR Engine
    TesseractOcr *m_tesseractOcr = nullptr;
    TesseractSettingsDialog *m_tesseractSettingsDialog = nullptr;
    QString m_tesseractSelectedLang = "";
    QString m_tesseractActiveLang = "";
    QStringList m_tesserractLangList;
    QString m_tesseractTessdataPath = "./tessdata";
    bool m_tesseractUseSystemTessdata = false;
    void initTesseractOCR();

    enum ProcessingMode {
        Auto,
        Manual
    };

    int m_tesseractMode = Auto;
    int m_tesseractAutoInterval = 1;

    int m_ollamaVisionMode = Manual;
    int m_ollamaVisionAutoInterval = 10;
    QTimer *m_ollamaVisionTimer = nullptr;
    QString m_ollamaVisionCacheOutput = "";
    bool m_waitForOllamaResponse = true;
    bool m_ollamaVisionRequestInProgress = false;

    // Replace Text
    QString replaceText(QString output);

    // Config
    QString m_currentRestoreToken;
    bool m_isCaptureDesktop = true;
    int m_currentDisplay = 0;
#ifdef __linux__
    unsigned long m_currentWindow = 0;
#else
    HWND m_currentWindow;
#endif
    QString m_initLanguage = "en_US";
#ifdef __linux__
    QString m_initHotKeyMode = "x11";
#endif
    void loadLogMessages();
    void loadConfig();
    void saveConfig();
};
#endif // MAINWINDOW_H
