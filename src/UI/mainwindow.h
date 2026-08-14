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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QNetworkProxy>
#include <QTableWidgetItem>

#include "src/controllers/pythoncontroller.h"
#include "src/utils/pluginloader.h"
#include "src/engines/opencv.h"
#include "tesseractsettingsdialog.h"
#include "ollamasettingsdialog.h"
#include "hooksettingsdialog.h"
#include "textoutputwindow.h"
#include "overlaywindow.h"
#include "previewwindow.h"

class TranslationController;
class HotkeyController;
class CaptureController;
class OcrController;
class HookController;
class ClipboardController;

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
    void showHistoryRequested();
    void screenCastFinished();
    void restartRequested();

private slots:
    void on_availableGeometryChanged();
    void on_buttonBox_clicked(QAbstractButton *button);

#ifdef Q_OS_LINUX
    void on_generalBindShortcut_clicked();
#endif
    void on_outputGeneralSelect_clicked();
    void on_outputToggledOriginalScreencast_stateChanged(int arg1);
    void on_outputToggledProcessedScreencast_stateChanged(int arg1);
    void on_outputToggledScreencast_stateChanged(int arg1);
    void on_outputProcessedOtsu_stateChanged(int arg1);
    void on_translatorOnlineGoogleSettingsButton_clicked();
    void on_textProcessingOCREngineToggled_stateChanged(int arg1);
    void on_textProcessingOCREngineTesseractSettingsButton_clicked();
    void on_textProcessingHookSettingsButton_clicked();
    void on_textProcessingAddRowButton_clicked();
    void on_textProcessingRemoveRowButton_clicked();

    // Configs (profiles)
    void on_configsNewButton_clicked();
    void on_configsLoadButton_clicked();
    void on_configsRenameButton_clicked();
    void on_configsDeleteButton_clicked();
    void on_pluginsReloadButton_clicked();
    void on_pluginsOpenDirectoryButton_clicked();

    // Python
    void on_pythonRecheckButton_clicked();
    void on_pythonSetupButton_clicked();
#ifdef Q_OS_WIN
    void on_pythonInstallPythonButton_clicked();
#endif
    void on_pythonInterpreterBrowseButton_clicked();
    void on_pythonOpenDirectoryButton_clicked();
    void on_pythonShowLogButton_clicked();
    void on_pythonComponentInstallButton_clicked();
    void on_pythonComponentRemoveButton_clicked();

    void on_logsNewLogMessage(const QString& message);
    void on_logsCopyAllButton_clicked();
    void on_logsOpenDirectoryButton_clicked();
    void on_proxyEnabledCheckBox_stateChanged(int arg1);

    // OpenCV
    void setCurrentOriginalFrame(const QImage &frame);
    void setCurrentProcessedFrame(const QImage &frame);
    void setCurrentProcessedMat(const cv::Mat &frame);

    // ScreenCast
    void startScreenCapture();
    void stopScreenCapture();

    // Utility Actions
    void setCurrentOutput(const QString &source, const QString &output);
    void openOllamaSettings();
    void retranslateText();
    void manualInjectHook();
    void reapplyHookOutput();

    // Output Window
    void selectNewRegion();
    void selectNewInnerRegion();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *m_manager;
    QScreen *m_screen;
    TextOutputWindow *m_outputWindow = nullptr;
    PluginManager *m_pluginManager;
    TranslationController *m_translationController = nullptr;

    // Clipboard
    ClipboardController *m_clipboardController = nullptr;

    // Python: shared by every optional feature that needs an interpreter
    PythonController *m_pythonController = nullptr;
    void initPythonController();
    void refreshPythonPage();
    void updatePythonButtons();
    QTableWidgetItem *selectedPythonRow() const;
    QString selectedPythonComponent() const;
    bool confirmPythonDownload(const PythonController::Component &component);

    void setupBaseUI();
    void initPlugins();
    void setupCoreConnections();
    void loadApplicationConfig();
    void initSubsystems();
    void initClipboardController();
    void setupSettingsConnections();
    void loadLogMessages();
    void setupFinalUI();
    void setupTextProcessingTable();

    QList<QObject*> m_changedWidgets;
    bool m_generalChanged = false;
    bool m_outputChanged = false;
    bool m_textProcessingChanged = false;
    bool m_translatorChanged = false;
    bool m_pluginChanged = false;
    bool m_proxyChanged = false;
    bool m_pythonChanged = false;

    bool widgetChanged(QWidget *widget);
    void setPropertyChanged(const bool &value);

    QMenu* createMenu(const QString &title, void (MainWindow::*slot)());
    QMap<QLabel*, QMenu*> contextMenus;
    void showContextMenu(const QPoint &pos);
    PreviewWindow* createPreviewWindow(const QString &title, void (OpenCV::*frameSignal)(const QImage&));
    void openOriginalPreview();
    void openProcessedPreview();

    // Global Shortcuts
    HotkeyController *m_hotkeyController = nullptr;
    bool m_isShortcuts = false;

    // Overlay
    OverlayWindow *m_overlayWindow = nullptr;
    QImage m_overlayImage;
    void captureRegion();
    void showHistory();
    void showOverlayWindow();

    // Screen Casting
    CaptureController *m_captureController = nullptr;
    OpenCV *m_opencv = nullptr;
    void initScreenCast();

    // Translator
    OcrController *m_ocrController = nullptr;
    OllamaSettingsDialog *m_ollamaSettingsDialog = nullptr;
    QString m_ollamaUrl = "http://localhost:11434/";
    QString m_ollamaCurrentModel;
    QString m_ollamaTranslationPrompt = "";
    QString m_ollamaVisionPrompt = "";
    QJsonArray m_ollamaModels;

    QString m_googleSourceLang;
    QString m_googleTargetLang;

    // OCR Engine
    enum OcrEngine {
        Tesseract,
        OllamaVision
    };

    OcrEngine m_ocrEngine = OcrEngine::Tesseract;

    TesseractSettingsDialog *m_tesseractSettingsDialog = nullptr;
    QString m_tesseractSelectedLang = "";
    QString m_tesseractActiveLang = "";
    QStringList m_tesserractLangList;
    QString m_tesseractTessdataPath = "./tessdata";
    bool m_tesseractUseSystemTessdata = false;

    enum ProcessingMode {
        Auto,
        Manual
    };

    int m_tesseractMode = Auto;
    int m_tesseractAutoInterval = 1;

    int m_ollamaVisionMode = Manual;
    int m_ollamaVisionAutoInterval = 10;
    bool m_waitForOllamaResponse = true;

    // Hook
    HookSettingsDialog *m_hookSettingsDialog = nullptr;
    HookSettingsDialog::HookMode m_hookMode = HookSettingsDialog::HookMode::GameAppMode;
    QMap<QString, QString> m_hookGameAppPluginList;
    QMap<QString, QString> m_hookEnginePluginList;
    QString m_currentGameAppPlugin;
    QString m_currentEnginePlugin;
    QString m_currentEngineProcess;
    HookController *m_hookController = nullptr;
    QList<PluginManager::PluginInfo> m_registry;
    QMap<QString, QString> m_currentHookTexts;
    QTimer *m_hookBurstTimer = nullptr;
    QMap<QString, QString> m_hookBurstBuffer;
    void flushHookBurst();

    // Replace Text
    enum TextProcessingColumn {
        ColRegex = 0,  // checkbox: treat the search string as a regular expression
        ColSource,     // which source the rule applies to (empty = all)
        ColFrom,       // string to search for
        ColTo          // string to replace with
    };

    struct ReplacementRule {
        bool regex = false;
        QString source;
        QString from;
        QString to;
        QString target;
    };

    QList<ReplacementRule> m_replacementRules;
    QSet<QString> m_gameScopedTargets;

    QString replaceText(const QString &source, QString output);
    QTableWidgetItem *makeRegexFlagItem(bool checked);
    QString currentRuleScope() const;
    void saveRuleScope();
    QString activeHookTargetKey() const;
    QString currentGameLabel() const;
    void markRulesChanged();
    void refreshRuleScopeBox();
    void populateReplacementTable();
    void commitReplacementTable();

    // Proxy
    QNetworkProxy::ProxyType m_proxyType = QNetworkProxy::HttpProxy;

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

    // Config
    void loadConfig();
    void loadGeneralSettings(const QJsonObject& general);
    void loadOutputSettings(const QJsonObject& output);
    void loadTranslatorSettings(const QJsonObject& translator);
    void loadTextProcessingSettings(const QJsonObject& textProcessing);
    void loadProxySettings(const QJsonObject& proxy);
    void loadPythonSettings(const QJsonObject& python);
    void loadScreencastSettings();

    void loadOcrSettings();
    void loadHookPluginSettings();
    void syncHookControllerTargets();

    QString currentHookTargetKey() const;
    void clearHookState();
    void syncPluginConfigs();
    QString buildPluginConfigJson(int flushMs) const;
    void pushCurrentPluginConfig();

    void openSpeedSettings();
    void updateSpeedButtonAvailability();
    int storedFlushMs(const QString &targetKey) const;

    void reapplyProfileSections();
    void refreshConfigsPage();
    void saveConfig();
};
#endif // MAINWINDOW_H
