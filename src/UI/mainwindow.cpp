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

#include <QMessageBox>
#include <QNetworkProxy>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QTimer>
#include <QJsonArray>
#include <QProcess>

#include "mainwindow.h"
#include "src/UI/forms/ui_mainwindow.h"
#include "ui_mainwindow.h"
#include "googlesettingsdialog.h"
#include "src/utils/logger.h"
#include "src/utils/config.h"
#include "src/data.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow)
    , m_screen(QGuiApplication::primaryScreen())
    , m_overlayWindow(new OverlayWindow())
    , m_outputWindow(new TextOutputWindow())
    , m_manager(new QNetworkAccessManager(this))
    , m_tesseractOcr(new TesseractOcr(this))
    , m_opencv(new OpenCV(this))
    , m_ollama(new Ollama(m_manager, this))
    , m_google(new Google(m_manager, this))
{
    ui->setupUi(this);
    ui->aboutLabel->setText("<span style=\"font-size: 18pt; font-weight: 700;\">" + QString::fromStdString(APP_NAME) + "</span>");
    ui->aboutVersion->setText(QString("v%1").arg(APP_VERSION));
    ui->aboutCopyrightLabel->setText(QString::fromStdString("Copyright (c) ") + QString::fromStdString(COPYRIGHT_YEARS) + QString::fromStdString(" Daniil Nabiulin"));
    ui->generalBoxLanguage->addItem("English", "en_US");
    ui->generalBoxLanguage->addItem("Русский", "ru_RU");

#ifdef Q_OS_WIN
    ui->generalLabelHotKey->hide();
    ui->generalRadioHotKey->hide();
    ui->generalRadioHotKeyPortal->hide();
    ui->generalBindShortcut->hide();
#endif
    connect(m_screen, &QScreen::availableGeometryChanged, this, &MainWindow::on_availableGeometryChanged);

    connect(ui->listSettingsWidget, &QListWidget::currentRowChanged, ui->settingsPages, &QStackedWidget::setCurrentIndex);
    connect(ui->outputProcessedSimpleThresh, &QRadioButton::toggled, m_opencv, &OpenCV::on_thresholdMethodChanged);
    connect(ui->outputProcessedSimpleThresholdingType, &QComboBox::currentIndexChanged, m_opencv, &OpenCV::on_thresholdSimpleTypeChanged);
    connect(ui->outputProcessedAdaptiveThresholdingType, &QComboBox::currentIndexChanged, m_opencv, &OpenCV::on_thresholdAdaptiveTypeChanged);
    connect(ui->outputProcessedOtsu, &QCheckBox::stateChanged, m_opencv, &OpenCV::on_otsuChanged);
    connect(ui->outputProcessedThreshValue, &QSlider::valueChanged, m_opencv, &OpenCV::setCurrentThresh);

    loadConfig();

    // Overlay Window
    m_overlayWindow->raise();
    connect(m_overlayWindow, &OverlayWindow::hideOverlay, this, [this] {
        m_overlayWindow->hide();
        m_outputWindow->show();
    });

    // Output Window
    m_outputWindow->show();
    connect(this, &MainWindow::on_showHistoryText, m_outputWindow, &TextOutputWindow::showHistory);
    connect(m_outputWindow, &TextOutputWindow::on_retranslate, this, &MainWindow::triggerManualOCR);
    connect(m_outputWindow, &TextOutputWindow::on_selectNewRegion, this, &MainWindow::selectNewRegionRequest);
    connect(m_outputWindow, &TextOutputWindow::on_selectNewInnerRegion, this, &MainWindow::selectNewInnerRegionRequest);

    m_ollamaSettingsDialog = new OllamaSettingsDialog(m_ollama, m_ollamaCurrentModel, m_ollamaModels, this);

    initHotKeys();
    initScreenCast();
    initTesseractOCR();

    // WidgetChanged
    connect(ui->generalBoxLanguage, &QComboBox::currentIndexChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalToggledStartup, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::keySequenceChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalRadioHotKey, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->generalRadioHotKeyPortal, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->outputToggledOriginalScreencast, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->outputToggledProcessedScreencast, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->outputGeneralBoxFramerate, &QComboBox::currentIndexChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->outputProcessedSimpleThresh, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->outputProcessedAdaptiveThresh, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->outputProcessedOtsu, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->outputProcessedSimpleThresholdingType, &QComboBox::currentIndexChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->outputProcessedThreshValue, &QSlider::valueChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->outputProcessedAdaptiveThresholdingType, &QComboBox::currentIndexChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->translatorOnlineGoogleToggled, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->translatorOfflineOllamaToggled, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->textProcessingOCREngineTesseractRadio, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->textProcessingTableWidget, &QTableWidget::currentItemChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyEnabledCheckBox, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyAddressEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyPortEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyUserEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyPasswordEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyTypeHttp, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyTypeSocks, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);

    // TranslatorChanged
    connect(ui->translatorOnlineGoogleToggled, &QCheckBox::stateChanged, this, &MainWindow::on_translatorChanged);
    connect(ui->translatorOfflineOllamaToggled, &QCheckBox::stateChanged, this, &MainWindow::on_translatorChanged);

    // Logs
    loadLogMessages();
}

MainWindow::~MainWindow()
{
    if (m_tesseractOcr) {
        m_tesseractOcr->stop();
        m_tesseractOcr->wait();
        delete m_tesseractOcr;
    }

    if (m_opencv) {
        m_opencv->setIsStopped(true);
        delete m_opencv;
    }

#ifdef Q_OS_LINUX
    if (m_pipewire) {
        m_pipewire->stop();
        delete m_pipewire;
    }
#endif

    if (m_screenCapture) {
        m_screenCapture->stop();
        delete m_screenCapture;
    }

    if (m_overlayWindow) { delete m_overlayWindow; }
    if (m_screenCastWindow) { delete m_screenCastWindow; }
    if (m_outputWindow) { delete m_outputWindow; }

    Logger::instance()->destroyInstance();
    Config::instance()->destroyInstance();

    delete m_manager;
    delete ui;
}

void MainWindow::on_availableGeometryChanged()
{
    m_outputWindow->move(m_screen->geometry().x() + 50, m_screen->geometry().y() + 50);
    m_overlayWindow->move(m_screen->geometry().x(), m_screen->geometry().y());

    if (!m_overlayWindow->isHidden()) {
        m_overlayWindow->hide();
        m_outputWindow->show();
    }
}

void MainWindow::on_widgetChanged()
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void MainWindow::on_translatorChanged()
{
    m_tesseractOcr->clearCache();
}

void MainWindow::on_buttonBox_clicked(QAbstractButton *button)
{
    QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);

    if (role == QDialogButtonBox::ApplyRole) {
        saveConfig();
    }

    loadConfig();
}

void MainWindow::on_portalShortcutActivated(const QString &shortcutId)
{
    if (shortcutId == "CaptureRegion") captureRegion();
    if (shortcutId == "HistoryTranslation") showHistory();
    if (shortcutId == "ManualTranslate") triggerManualOCR();
}

void MainWindow::on_portalShortcutDeactivated()
{
    if (m_isShortcuts) {
        m_isShortcuts = false;
    }
}

#ifdef Q_OS_LINUX
void MainWindow::on_generalBindShortcut_clicked()
{
    m_portalHotKeys->bindShortcuts();
}
#endif

void MainWindow::on_outputGeneralSelect_clicked()
{
#ifdef Q_OS_LINUX
    if (m_portalScreencast) {
        if (m_opencv) {
            m_opencv->setIsStopped(true);
            ui->outputOriginalScreencast->clear();
            ui->outputProcessedScreencast->clear();
            m_overlayWindow->clearFrame();
            m_overlayImage = QImage();
        }

        if (m_pipewire) {
            m_pipewire->stop();
        }

        m_portalScreencast->reload();
    } else {
#endif
        m_screenCastWindow->show();
#ifdef Q_OS_LINUX
    }
#endif
}

void MainWindow::on_outputProcessedOtsu_stateChanged(int arg1)
{
    ui->outputProcessedThreshValue->setEnabled(!arg1);
}

void MainWindow::on_translatorOnlineGoogleSettingsButton_clicked()
{
    GoogleSettingsDialog *dialog = new GoogleSettingsDialog(m_googleSourceLang, m_googleTargetLang, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if (result == QDialog::Accepted) {
            m_googleSourceLang = dialog->getSourceLang();
            m_googleTargetLang = dialog->getTargetLang();
            m_google->setSourceLang(m_googleSourceLang);
            m_google->setTargetLang(m_googleTargetLang);

            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
    });

    dialog->show();
}

void MainWindow::on_textProcessingOCREngineTesseractSettingsButton_clicked()
{
    const QString status = QStringLiteral("<b>%1%2</b>").arg(
        m_tesseractOcr->isRunning() ? tr("Active") : tr("Inactive"),
        m_tesseractActiveLang.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(m_tesseractActiveLang)
    );

    m_tesseractSettingsDialog = new TesseractSettingsDialog(status,
                                                            m_tesseractSelectedLang,
                                                            m_tesserractLangList,
                                                            m_tesseractTessdataPath,
                                                            m_tesseractUseSystemTessdata,
                                                            m_tesseractMode,
                                                            m_tesseractAutoInterval,
                                                            m_tesseractOcr,
                                                            this);

    m_tesseractSettingsDialog->setWindowModality(Qt::WindowModal);
    m_tesseractSettingsDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(m_tesseractSettingsDialog, &QDialog::finished, this, [this](int result) {
        if (result == QDialog::Accepted) {
            m_tesseractSelectedLang = m_tesseractSettingsDialog->getCurrentLanguage();
            m_tesserractLangList = m_tesseractSettingsDialog->getLanguageList();
            m_tesseractUseSystemTessdata = m_tesseractSettingsDialog->getUseSystemTessdata();
            m_tesseractTessdataPath = m_tesseractSettingsDialog->getTessdataPath();
            m_tesseractMode = m_tesseractSettingsDialog->getMode();
            m_tesseractAutoInterval = m_tesseractSettingsDialog->getAutoInterval();

            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
    });

    m_tesseractSettingsDialog->show();
}

void MainWindow::on_logsNewLogMessage(const QString& message)
{
    ui->logsPlainText->appendPlainText(message);
}

void MainWindow::on_logsCopyAllButton_clicked()
{
    QApplication::clipboard()->setText(ui->logsPlainText->toPlainText());
}

void MainWindow::on_logsOpenDirectoryButton_clicked()
{
    QDir dir(Logger::getLogDirPath());
    if (!dir.isEmpty()) {
        QString path = dir.path();
        QUrl url = QUrl::fromLocalFile(path);
        QDesktopServices::openUrl(url);
    }
}

void MainWindow::on_proxyEnabledCheckBox_stateChanged(int arg1)
{
    bool enabled = (arg1 == Qt::Unchecked);

    ui->proxyIPLabel->setEnabled(enabled);
    ui->proxyPortLabel->setEnabled(enabled);
    ui->proxyUserLabel->setEnabled(enabled);
    ui->proxyPasswordLabel->setEnabled(enabled);
    ui->proxyAddressEdit->setEnabled(enabled);
    ui->proxyPortEdit->setEnabled(enabled);
    ui->proxyUserEdit->setEnabled(enabled);
    ui->proxyPasswordEdit->setEnabled(enabled);
    ui->proxyTypeHttp->setEnabled(enabled);
    ui->proxyTypeSocks->setEnabled(enabled);
}

void MainWindow::setCurrentRestoreToken(const QString &restoreToken)
{
    QJsonObject screencast;
    m_currentRestoreToken = restoreToken;
    screencast["restore_token"] = m_currentRestoreToken;
    Config::setValue("screencast_portal", screencast);
    Config::saveConfig("settings.json");
}

void MainWindow::setCurrentNodeId(const uint &nodeId)
{
#ifdef Q_OS_LINUX
    Log(Logger::Level::Info, "[pipewire] Source selected");
    m_pipewire->init(nodeId);
    m_pipewire->start();
    m_pipewire->setIsStopped(false);
    m_opencv->setIsStopped(false);
#endif
}

void MainWindow::setCurrentOriginalFrame(const QImage &frame)
{
    m_overlayImage = frame.copy();
    m_overlayImage.setDevicePixelRatio(this->devicePixelRatio());

    if (ui->listSettingsWidget->currentRow() == 1 && !frame.isNull()) {
        ui->outputOriginalScreencast->setPixmap(QPixmap::fromImage(m_overlayImage).scaled(ui->outputOriginalScreencast->size() * this->devicePixelRatio(), Qt::KeepAspectRatio));
    }
#ifdef Q_OS_LINUX
    if (m_pipewire) {
        m_pipewire->setIsProcessed(true);
        m_pipewire->wakeWaitCondition();
    }
#endif

    if (m_screenCapture) {
        m_screenCapture->setIsProcessed(true);
        m_screenCapture->wakeWaitCondition();
    }
}

void MainWindow::setCurrentProcessedFrame(const QImage &frame)
{
    QImage image = frame;
    image.setDevicePixelRatio(this->devicePixelRatio());

    if (ui->listSettingsWidget->currentRow() == 1 && !frame.isNull()) {
        ui->outputProcessedScreencast->setPixmap(QPixmap::fromImage(image).scaled(ui->outputProcessedScreencast->size() * this->devicePixelRatio(), Qt::KeepAspectRatio));
    }
}

void MainWindow::setCurrentProcessedMat(const cv::Mat &frame)
{
    if (frame.empty())
        return;

    if (ui->textProcessingOCREngineTesseractRadio->isChecked())
        m_tesseractOcr->frameMat(frame);
    else
        m_ollama->frameMat(frame);
}

void MainWindow::setCurrentOutputOCR(const QString &output)
{
    QString outputFilt = replaceText(output);

    // Google
    if (ui->translatorOnlineGoogleToggled->isChecked()) {
        m_google->translateText(outputFilt, [this, outputFilt](QString result) {
            emit currentOverlayText("[Google]", outputFilt, result + "\n");
        });
    } else {
        emit clearOverlayText("[Google]");
    }

    // Ollama
    if (ui->translatorOfflineOllamaToggled->isChecked()) {
        QJsonObject ollama = Config::getValue("ollama").toJsonObject();
        m_ollama->generate(ollama["translation_prompt"].toString() + outputFilt, m_ollamaCurrentModel, [this, outputFilt](QString result){
            emit currentOverlayText("[Ollama]", outputFilt, result + "\n");
        });
    } else {
        emit clearOverlayText("[Ollama]");
    }
}

void MainWindow::on_outputToggledOriginalScreencast_stateChanged(int arg1)
{
    ui->outputOriginalScreencast->setVisible(arg1 == Qt::Checked);
    ui->outputOriginalScreencastLabel->setVisible(arg1 == Qt::Checked);
}

void MainWindow::on_outputToggledProcessedScreencast_stateChanged(int arg1)
{
    ui->outputProcessedScreencast->setVisible(arg1 == Qt::Checked);
    ui->outputProcessedScreencastLabel->setVisible(arg1 == Qt::Checked);
}

void MainWindow::on_textProcessingAddRowButton_clicked()
{
    int row = ui->textProcessingTableWidget->rowCount();
    ui->textProcessingTableWidget->insertRow(row);
}

void MainWindow::on_textProcessingRemoveRowButton_clicked()
{
    int row = ui->textProcessingTableWidget->currentRow();

    if (row >= 0) {
        ui->textProcessingTableWidget->removeRow(row);
    }
}

void MainWindow::openOllamaSettings()
{
    m_ollamaSettingsDialog->setCurrentSettings(m_ollamaUrl, m_ollamaCurrentModel, m_ollamaTranslationPrompt, m_ollamaVisionPrompt, m_ollamaVisionMode, m_ollamaVisionAutoInterval, m_waitForOllamaResponse);
    m_ollamaSettingsDialog->setWindowModality(Qt::WindowModal);

    connect(m_ollamaSettingsDialog, &QDialog::finished, this, [this](int result) {
        if (result == QDialog::Accepted) {
            m_ollamaUrl = m_ollamaSettingsDialog->getUrl();
            m_ollamaCurrentModel = m_ollamaSettingsDialog->getCurrentModel();
            m_ollamaModels = m_ollamaSettingsDialog->getModels();
            m_ollamaTranslationPrompt = m_ollamaSettingsDialog->getTranslationPrompt();
            m_ollamaVisionPrompt = m_ollamaSettingsDialog->getVisionPrompt();
            m_ollamaVisionMode = m_ollamaSettingsDialog->getMode();
            m_ollamaVisionAutoInterval = m_ollamaSettingsDialog->getAutoInterval();
            m_waitForOllamaResponse = m_ollamaSettingsDialog->getIsWaitForResponse();

            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
    });

    m_ollamaSettingsDialog->show();
}

void MainWindow::ollamaVisionTimerTimeout()
{
    if (m_waitForOllamaResponse && m_ollamaVisionRequestInProgress) {
        return;
    }

    m_ollamaVisionRequestInProgress = true;

    m_ollama->generateVision(m_ollamaVisionPrompt, m_ollamaCurrentModel, [this](QString result) {
        m_ollamaVisionRequestInProgress = false;

        if (m_ollamaVisionCacheOutput == result || result.isEmpty()) {
            return;
        }

        m_ollamaVisionCacheOutput = result;
        setCurrentOutputOCR(result);
    });
}

void MainWindow::triggerManualOCR()
{    
    if (ui->textProcessingOCREngineTesseractRadio->isChecked()) {
        m_tesseractOcr->clearCache();
        m_tesseractOcr->triggerManualOCR();
    }
    else {
        m_ollama->generateVision(m_ollamaVisionPrompt, m_ollamaCurrentModel, [this](QString result){
            setCurrentOutputOCR(result);
        });
    }
}

void MainWindow::selectNewRegionRequest()
{
    if (!m_overlayImage.isNull()) {
        m_overlayWindow->setInnerBrushActive(false);
        showOverlayWindow();
    }
}
void MainWindow::selectNewInnerRegionRequest()
{
    if (!m_overlayWindow->getIsRectBrushEmpty()) {
        m_overlayWindow->setInnerBrushActive(true);
        showOverlayWindow();
    }
}

void MainWindow::initHotKeys()
{
    if (ui->generalRadioHotKey->isChecked()) {
        m_captureRegionHotKey = new HotKeys(this);
        m_captureRegionHotKey->setShortcut(ui->generalHotkeySelectNewRegionEdit->keySequence());
        connect(m_captureRegionHotKey, &HotKeys::activated, this, &MainWindow::captureRegion);
        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged, m_captureRegionHotKey, &HotKeys::setShortcut);
        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::editingFinished, this, [this] {
            ui->generalHotkeySelectNewRegionEdit->clearFocus();
        });

        m_showHistoryTranslationHotKey = new HotKeys(this);
        m_showHistoryTranslationHotKey->setShortcut(ui->generalHotkeyHistoryTranslationEdit->keySequence());
        connect(m_showHistoryTranslationHotKey, &HotKeys::activated, this, &MainWindow::showHistory);
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged, m_showHistoryTranslationHotKey, &HotKeys::setShortcut);
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::editingFinished, this, [this] {
            ui->generalHotkeyHistoryTranslationEdit->clearFocus();
        });

        m_manualTranslateHotKey = new HotKeys(this);
        m_manualTranslateHotKey->setShortcut(ui->generalHotkeyManualTranslateEdit->keySequence());
        connect(m_manualTranslateHotKey, &HotKeys::activated, this, &MainWindow::triggerManualOCR);
        connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::keySequenceChanged, m_manualTranslateHotKey, &HotKeys::setShortcut);
        connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::editingFinished, this, [this] {
            ui->generalHotkeyManualTranslateEdit->clearFocus();
        });
    }
#ifdef Q_OS_LINUX
    else {
        m_portalHotKeys = new PortalHotkeys(this);
        m_portalHotKeys->init();
        connect(m_portalHotKeys, &PortalHotkeys::activated, this, &MainWindow::on_portalShortcutActivated);
        connect(m_portalHotKeys, &PortalHotkeys::deactivated, this, &MainWindow::on_portalShortcutDeactivated);
    }
#endif
}

void MainWindow::captureRegion()
{
    if (m_overlayWindow->isHidden() && (!m_isShortcuts || ui->generalRadioHotKey->isChecked())) {
        m_isShortcuts = true;

        if (m_overlayImage.isNull()) {
            QMessageBox::warning(this, tr("Warning"), tr("No screencast selected for OCR"));
            return;
        }
        m_overlayWindow->setInnerBrushActive(false);
        showOverlayWindow();
    }
}

void MainWindow::showHistory()
{
    if (!m_isShortcuts || ui->generalRadioHotKey->isChecked()) {
        m_isShortcuts = true;

        emit on_showHistoryText();
    }
}

void MainWindow::showOverlayWindow()
{
    QRect primaryScreenGeometry = QApplication::primaryScreen()->geometry();
    m_overlayWindow->setPixmap(QPixmap::fromImage(m_overlayImage.copy().scaled(primaryScreenGeometry.width() * this->devicePixelRatio(), primaryScreenGeometry.height() * this->devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    m_overlayWindow->move(m_screen->geometry().x(), m_screen->geometry().y());
    m_outputWindow->hide();
    m_overlayWindow->showFullScreen();
}

QString MainWindow::replaceText(QString output)
{
    for (int i = 0; i < ui->textProcessingTableWidget->rowCount(); ++i) {
        QString from = ui->textProcessingTableWidget->item(i, 0)->text();
        QString to = ui->textProcessingTableWidget->item(i, 1)->text();
        output.replace(from, to);
    }

    return output;
}

void MainWindow::initTesseractOCR()
{
    connect(m_tesseractOcr, &TesseractOcr::currentOutputOCR, this, &MainWindow::setCurrentOutputOCR);
    connect(this, &MainWindow::currentOverlayText, m_outputWindow, &TextOutputWindow::setCurrentOutputOCR);
    connect(this, &MainWindow::clearOverlayText, m_outputWindow, &TextOutputWindow::clearOverlayText);
}

void MainWindow::initScreenCast()
{
#ifdef Q_OS_LINUX
    m_pipewire = new Pipewire();
    m_pipewire->setCurrentFramerate(ui->outputGeneralBoxFramerate->currentText());

    connect(ui->outputGeneralBoxFramerate, &QComboBox::currentTextChanged, m_pipewire, &Pipewire::setCurrentFramerate);
    connect(m_pipewire, &Pipewire::currentFrameBuffer, m_opencv, &OpenCV::setCurrentFrameBuffer);
#endif
    connect(m_opencv, &OpenCV::currentOriginalFrame, this, &MainWindow::setCurrentOriginalFrame);
    connect(m_opencv, &OpenCV::currentProcessedFrame, this, &MainWindow::setCurrentProcessedFrame);
    connect(m_opencv, &OpenCV::currentProcessedMat, this, &MainWindow::setCurrentProcessedMat);
    connect(m_overlayWindow, &OverlayWindow::currentRoi, m_opencv, &OpenCV::setCurrentRoi);
    connect(m_overlayWindow, &OverlayWindow::currentInnerRoi, m_opencv, &OpenCV::setCurrentIgnoreRoi);
#ifdef Q_OS_LINUX
    m_portalScreencast = new ScreenCastPortal(m_currentRestoreToken);
    connect(m_portalScreencast, &ScreenCastPortal::currentRestoreToken, this, &MainWindow::setCurrentRestoreToken);
    connect(m_portalScreencast, &ScreenCastPortal::currentNodeId, this, &MainWindow::setCurrentNodeId);

    connect(m_portalScreencast, &ScreenCastPortal::failedPortal, this, [this]
    {
        delete m_portalScreencast; m_portalScreencast = nullptr;
        delete m_pipewire; m_pipewire = nullptr;
#endif
        m_screenCapture = new ScreenCast();
        m_screenCapture->setIsCaptureDesktop(m_isCaptureDesktop);

        m_screenCastWindow = new ScreenCastWindow(m_screenCapture);

        connect(m_screenCapture, &ScreenCast::currentFrameBuffer, m_opencv, &OpenCV::setCurrentFrameBuffer);
        connect(ui->outputGeneralBoxFramerate, &QComboBox::currentTextChanged, m_screenCapture, &ScreenCast::setCurrentFramerate);
        connect(m_screenCastWindow, &ScreenCastWindow::screencastWindowShown, this, [this] {
            connect(m_opencv, &OpenCV::currentOriginalFrame, m_screenCastWindow, &ScreenCastWindow::setCurrentOriginalFrame);
        });
        connect(m_screenCastWindow, &ScreenCastWindow::screencastWindowHidden, this, [this] {
            disconnect(m_opencv, &OpenCV::currentOriginalFrame, m_screenCastWindow, &ScreenCastWindow::setCurrentOriginalFrame);
        });

        if (m_isCaptureDesktop) {
            m_screenCapture->setCurrentDisplayIndex(m_currentDisplay);
        } else {
            m_screenCapture->setCurrentWindow(m_currentWindow);
        }

        m_screenCapture->start();
#ifdef Q_OS_LINUX
    });

    m_portalScreencast->init();
#endif
}

void MainWindow::loadLogMessages()
{
    QFile file(Logger::getLogFilePath());
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&file);
        QString line;
        while (in.readLineInto(&line)) {
            ui->logsPlainText->appendPlainText(line);
        }
        file.close();
    }

    connect(Logger::instance(), &Logger::newLogMessage, this, &MainWindow::on_logsNewLogMessage);
}

void MainWindow::loadConfig()
{
    // General
    QJsonObject general = Config::getValue("general").toJsonObject();
    m_initLanguage = general["language"].toString();
    int index = ui->generalBoxLanguage->findData(m_initLanguage);
    if (index != -1) {
        ui->generalBoxLanguage->setCurrentIndex(index);
    }
    if (!general["settings_startup"].isNull()) {
        bool hideStartup = general["settings_startup"].toBool();
        ui->generalToggledStartup->setChecked(hideStartup);
        if (!hideStartup) {
            show();
        }
    } else {
        show();
    }

    ui->generalHotkeySelectNewRegionEdit->setKeySequence(QKeySequence(general["hotkey_select_region"].toString()));
    ui->generalHotkeyHistoryTranslationEdit->setKeySequence(QKeySequence(general["hotkey_history_translation"].toString()));
    ui->generalHotkeyManualTranslateEdit->setKeySequence(QKeySequence(general["hotkey_manual_translate"].toString()));

#ifdef Q_OS_LINUX
    if (!general["hotkeys_type"].toString().isEmpty()) {
        m_initHotKeyMode = general["hotkeys_type"].toString();

        bool isX11Mode = (m_initHotKeyMode == "x11");
        ui->generalRadioHotKey->setChecked(isX11Mode);
        ui->generalRadioHotKeyPortal->setChecked(!isX11Mode);
        ui->generalHotkeySelectNewRegionEdit->setEnabled(isX11Mode);
        ui->generalHotkeyHistoryTranslationEdit->setEnabled(isX11Mode);
        ui->generalHotkeyManualTranslateEdit->setEnabled(isX11Mode);
        ui->generalBindShortcut->setEnabled(!isX11Mode);
    }
#endif

    // Output
    QJsonObject output = Config::getValue("output").toJsonObject();
    if (!output.empty()) {
        ui->outputToggledOriginalScreencast->setChecked(output["original_screencast_output"].toBool());
        ui->outputToggledProcessedScreencast->setChecked(output["processed_screencast_output"].toBool());
        ui->outputGeneralBoxFramerate->setCurrentIndex(output["framerate_index"].toInt());
    }

    // processing
    QJsonObject processing = output["processing"].toObject();
    if (!processing.empty()) {
        ui->outputProcessedSimpleThresh->setChecked(processing["is_simple_thresholding"].toBool());
        ui->outputProcessedAdaptiveThresh->setChecked(processing["is_adaptive_thresholding"].toBool());
        ui->outputProcessedOtsu->setChecked(processing["is_otsu_binarization"].toBool());
        ui->outputProcessedSimpleThresholdingType->setCurrentIndex(processing["simple_threshold_type"].toInt());
        ui->outputProcessedThreshValue->setValue(processing["threshold_value"].toInt());
        ui->outputProcessedAdaptiveThresholdingType->setCurrentIndex(processing["adaptive_method"].toInt());
    }

    // Screencast
    QJsonObject screencast = Config::getValue("screencast").toJsonObject();
    if (!screencast.empty()) {
        m_isCaptureDesktop = screencast["is_capture_desktop"].toBool();

        if (m_isCaptureDesktop) {
            m_currentDisplay = (screencast["display_index"].toInt());
        } else {
#ifdef Q_OS_LINUX
            m_currentWindow = (screencast["window_id"].toInt());
#elif defined(Q_OS_WIN)
            uintptr_t hwndValue = static_cast<uintptr_t>(screencast["window_id"].toVariant().toULongLong());
            m_currentWindow = reinterpret_cast<HWND>(hwndValue);
#endif
        }
#ifdef Q_OS_LINUX
    }

    QJsonObject screencast_portal = Config::getValue("screencast_portal").toJsonObject();
    if (!screencast_portal.empty()) {
        QString keyStr = screencast_portal["restore_token"].toString().trimmed();
        Config::setValue("screencast_portal", screencast_portal);

        m_currentRestoreToken = keyStr;
#endif
    }

    // Translator Online
    QJsonObject translator_online = Config::getValue("translator_online").toJsonObject();
    if (!translator_online.isEmpty()) {
        QJsonObject google = translator_online["google"].toObject();
        ui->translatorOnlineGoogleToggled->setChecked(google["is_google"].toBool());
        m_googleSourceLang = google["google_source_lang"].toString();
        m_googleTargetLang = google["google_target_lang"].toString();
        m_google->setSourceLang(m_googleSourceLang);
        m_google->setTargetLang(m_googleTargetLang);
    }

    // Translator Offline
    QJsonObject translator_offline = Config::getValue("translator_offline").toJsonObject();
    if (!translator_offline.isEmpty()) {
        ui->translatorOfflineOllamaToggled->setChecked(translator_offline["is_ollama_translator"].toBool());
    }

    // Text Processing (Ollama Vision && Tesseract)
    QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();
    if (!textProcessing.isEmpty()) {
        QJsonObject tesseract = textProcessing["tesseract"].toObject();
        ui->textProcessingOCREngineTesseractRadio->setChecked(tesseract["is_tesseract"].toBool());
        m_tesseractActiveLang = tesseract.value("lang").toString();
        m_tesseractUseSystemTessdata = tesseract["is_systemdata"].toBool();
        m_tesseractTessdataPath = tesseract["path_tessdata"].toString();
        m_tesseractMode = tesseract["mode"].toInt();
        m_tesseractAutoInterval = tesseract["delay"].toDouble();
        m_tesseractSelectedLang = m_tesseractActiveLang;

        m_tesseractOcr->setMode(m_tesseractMode);
        m_tesseractOcr->setDelay(m_tesseractAutoInterval);

        if (ui->textProcessingOCREngineTesseractRadio->isChecked()) {
            if (m_tesseractOcr->isRunning()) {
                m_tesseractOcr->stop();
            }

            if (m_tesseractUseSystemTessdata) {
                m_tesseractOcr->setTessdataPath(QString());
                QString tessdataPath = QString();
            } else {
                QString tessdataPath = m_tesseractTessdataPath;
                QDir dir(tessdataPath);
                if (dir.exists()) {
                    m_tesseractOcr->setTessdataPath(tessdataPath);
                } else {
                    m_tesseractUseSystemTessdata = true;
                    Log(Logger::Level::Warning, "[tesseract] The specified Tesseract data directory does not exist or is invalid");
                }
            }
            std::vector<std::string> languages = m_tesseractOcr->checkAvailableLanguages();
            m_tesserractLangList.clear();
            for (const auto& language : languages) {
                m_tesserractLangList << QString::fromStdString(language);
            }

            const QString language = m_tesseractActiveLang;
            if (!language.isEmpty()) {
                m_tesseractOcr->init(language);
            }
        }

        // Ollama Vision
        QJsonObject ollama_vision = textProcessing["ollama_vision"].toObject();
        ui->textProcessingOCREngineOllamaVisionRadio->setChecked(ollama_vision["is_vision"].toBool());
        m_ollamaVisionPrompt = ollama_vision["prompt"].toString();
        m_ollamaVisionMode = ollama_vision["mode"].toInt(Manual);
        m_ollamaVisionAutoInterval = ollama_vision["delay"].toInt(10);

        if (ui->textProcessingOCREngineOllamaVisionRadio->isChecked() && m_tesseractOcr->isRunning())
        {
            m_tesseractOcr->stop();
        }

        if (ui->textProcessingOCREngineOllamaVisionRadio->isChecked() && m_ollamaVisionMode == Auto) {
            if (!m_ollamaVisionTimer) {
                m_ollamaVisionTimer = new QTimer(this);
                connect(m_ollamaVisionTimer, &QTimer::timeout, this, &MainWindow::ollamaVisionTimerTimeout);
            }
            m_ollamaVisionTimer->setInterval(m_ollamaVisionAutoInterval * 1000);
            m_ollamaVisionTimer->start();
        } else {
            if (m_ollamaVisionTimer) {
                m_ollamaVisionTimer->stop();
                delete m_ollamaVisionTimer;
                m_ollamaVisionTimer = nullptr;
            }
        }

        QJsonArray jsonArray = textProcessing["text_replacement_table"].toArray();
        ui->textProcessingTableWidget->setRowCount(jsonArray.size());
        for (int i = 0; i < jsonArray.size(); i++) {
            QJsonObject rowObject = jsonArray[i].toObject();
            for (int j = 0; j < rowObject.size(); j++) {
                QString value = rowObject[QString::number(j)].toString();
                QTableWidgetItem *newItem = new QTableWidgetItem(value);
                ui->textProcessingTableWidget->setItem(i, j, newItem);
            }
        }
    }

    // Ollama
    QJsonObject ollama = Config::getValue("ollama").toJsonObject();
    if (!ollama.isEmpty()) {
        QString ollamaUrl = ollama["url"].toString();
        if (ollamaUrl != "") {m_ollamaUrl = ollamaUrl; m_ollama->setUrl(m_ollamaUrl); }
        m_ollamaCurrentModel = ollama["current_model"].toString();
        m_ollamaModels = ollama["models"].toArray();
        m_ollamaTranslationPrompt = ollama["translation_prompt"].toString();
        m_waitForOllamaResponse = ollama["wait_for_responce"].toBool();
    }

    // Proxy
    QJsonObject proxy = Config::getValue("proxy").toJsonObject();
    if (!proxy.isEmpty()) {
        ui->proxyEnabledCheckBox->setChecked(proxy["is_proxy"].toBool());
        ui->proxyAddressEdit->setText(proxy["ip"].toString());
        ui->proxyPortEdit->setText(proxy["port"].toString());
        ui->proxyUserEdit->setText(proxy["user"].toString());
        ui->proxyPasswordEdit->setText(proxy["password"].toString());
        if (proxy["type"] == "http") {
            ui->proxyTypeHttp->setChecked(true);
        } else if (proxy["type"] == "socks") {
            ui->proxyTypeSocks->setChecked(true);
        }
    }

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void MainWindow::saveConfig()
{
    // General
    QJsonObject general;
    general["language"] = ui->generalBoxLanguage->currentData().toString();
    general["settings_startup"] = ui->generalToggledStartup->isChecked();

#ifdef Q_OS_LINUX
    if (ui->generalRadioHotKey->isChecked()) {
        general["hotkeys_type"] = "x11";
    } else if (ui->proxyTypeSocks->isChecked()) {
        general["hotkeys_type"] = "portal";
    }
#endif

    general["hotkey_select_region"] = ui->generalHotkeySelectNewRegionEdit->keySequence().toString();
    general["hotkey_history_translation"] = ui->generalHotkeyHistoryTranslationEdit->keySequence().toString();
    general["hotkey_manual_translate"] = ui->generalHotkeyManualTranslateEdit->keySequence().toString();
    Config::setValue("general", general);

    // Output
    QJsonObject output;
    output["original_screencast_output"] = ui->outputToggledOriginalScreencast->isChecked();
    output["processed_screencast_output"] = ui->outputToggledProcessedScreencast->isChecked();
    output["framerate_index"] = ui->outputGeneralBoxFramerate->currentIndex();

    // Processing
    QJsonObject processing;
    processing["is_simple_thresholding"] = ui->outputProcessedSimpleThresh->isChecked();
    processing["is_adaptive_thresholding"] = ui->outputProcessedAdaptiveThresh->isChecked();
    processing["is_otsu_binarization"] = ui->outputProcessedOtsu->isChecked();
    processing["simple_threshold_type"] = ui->outputProcessedSimpleThresholdingType->currentIndex();
    processing["threshold_value"] = ui->outputProcessedThreshValue->value();
    processing["adaptive_method"] = ui->outputProcessedAdaptiveThresholdingType->currentIndex();

    output["processing"] = processing;
    Config::setValue("output", output);

    // Translator Online
    QJsonObject translator_online, google;
    google["is_google"] = ui->translatorOnlineGoogleToggled->isChecked();
    google["google_source_lang"] = m_googleSourceLang;
    google["google_target_lang"] = m_googleTargetLang;
    translator_online.insert("google", google);
    Config::setValue("translator_online", translator_online);

    // Translator Offline
    QJsonObject translator_offline;
    translator_offline["is_ollama_translator"] = ui->translatorOfflineOllamaToggled->isChecked();
    Config::setValue("translator_offline", translator_offline);

    // Text Processing (Tesseract & Ollama Vision)
    QJsonObject textProcessing, tesseract, ollama_vision;
    tesseract["is_tesseract"] = ui->textProcessingOCREngineTesseractRadio->isChecked();
    tesseract["lang"] = m_tesseractSelectedLang;
    tesseract["is_systemdata"] = m_tesseractUseSystemTessdata;
    tesseract["path_tessdata"] = m_tesseractTessdataPath;
    tesseract["mode"] = m_tesseractMode;
    tesseract["delay"] = m_tesseractAutoInterval;

    ollama_vision["is_vision"] = ui->textProcessingOCREngineOllamaVisionRadio->isChecked();
    ollama_vision["prompt"] = m_ollamaVisionPrompt;
    ollama_vision["mode"] = m_ollamaVisionMode;
    ollama_vision["delay"] = m_ollamaVisionAutoInterval;

    textProcessing.insert("tesseract", tesseract);
    textProcessing.insert("ollama_vision", ollama_vision);

    QJsonArray jsonArray;
    for (int i = 0; i < ui->textProcessingTableWidget->rowCount(); i++) {
        QJsonObject rowObject;
        for (int j = 0; j < ui->textProcessingTableWidget->columnCount(); j++) {
            QTableWidgetItem *item = ui->textProcessingTableWidget->item(i, j);
            if (item != nullptr) {
                rowObject[QString::number(j)] = item->text();
            }
        }
        jsonArray.append(rowObject);
    }
    textProcessing["text_replacement_table"] = jsonArray;
    Config::setValue("text_processing", textProcessing);

    // Ollama
    QJsonObject ollama;
    ollama["url"] = m_ollamaUrl;
    ollama["current_model"] = m_ollamaCurrentModel;
    ollama["models"] = m_ollamaModels;
    ollama["translation_prompt"] = m_ollamaTranslationPrompt;
    ollama["wait_for_responce"] = m_waitForOllamaResponse;
    Config::setValue("ollama", ollama);

    // Proxy
    QJsonObject proxy;
    proxy["is_proxy"] = ui->proxyEnabledCheckBox->isChecked();
    proxy["ip"] = ui->proxyAddressEdit->text();
    proxy["port"] = ui->proxyPortEdit->text();
    proxy["user"] = ui->proxyUserEdit->text();
    proxy["password"] = ui->proxyPasswordEdit->text();

    QNetworkProxy::ProxyType proxyType;
    if (ui->proxyTypeHttp->isChecked()) {
        proxy["type"] = "http";
        proxyType = QNetworkProxy::HttpProxy;
    } else if (ui->proxyTypeSocks->isChecked()) {
        proxy["type"] = "socks";
        proxyType = QNetworkProxy::Socks5Proxy;
    }
    Config::setValue("proxy", proxy);

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    // Save Config
    Config::saveConfig("settings.json");

    // Proxy
    if (ui->proxyEnabledCheckBox->isChecked())
    {
        QString ip = ui->proxyAddressEdit->text();
        QString port = ui->proxyPortEdit->text();
        QString user = ui->proxyUserEdit->text();
        QString password = ui->proxyPasswordEdit->text();

        QNetworkProxy proxy;
        proxy.setType(proxyType);
        proxy.setHostName(ip);
        proxy.setPort(port.toInt());

        if(!user.isEmpty()) {
            proxy.setUser(user);
            proxy.setPassword(password);
        }

        QNetworkProxy::setApplicationProxy(proxy);
    } else {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }

#ifdef Q_OS_LINUX
    if (m_initHotKeyMode != general["hotkeys_type"].toString() || m_initLanguage != general["language"].toString()) {
#elif defined(Q_OS_WIN)
    if (m_initLanguage != general["language"].toString() && !general.isEmpty()) {
#endif
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Restart Required"));
        msgBox.setText(tr("Your changes will take effect the next time you start AurexTranslator."));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setButtonText(QMessageBox::Yes, tr("Restart Now"));
        msgBox.setButtonText(QMessageBox::No, tr("Later"));

        int ret = msgBox.exec();
        switch (ret) {
        case QMessageBox::Yes:
            QApplication::quit();
            QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
            break;
        case QMessageBox::No:
            break;
        default:
            break;
        }
    }
}
