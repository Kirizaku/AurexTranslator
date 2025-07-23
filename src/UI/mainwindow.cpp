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

#include <QMessageBox>
#include <QNetworkProxy>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonArray>
#include <QProcess>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "src/translations/ollamasettingsdialog.h"
#include "src/translations/googlesettingsdialog.h"
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
    , m_ollama(new Ollama(m_manager))
    , m_google(new Google(m_manager))
{
    ui->setupUi(this);
    ui->aboutLabel->setText("<span style=\"font-size: 18pt; font-weight: 700;\">" + QString::fromStdString(APP_NAME) + "</span>");
    ui->aboutVersion->setText(QString("v%1").arg(APP_VERSION));
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

    m_overlayWindow->raise();
    connect(m_overlayWindow, &OverlayWindow::hideOverlay, this, [this] {
        m_overlayWindow->hide();
        m_outputWindow->show();
    });

    m_outputWindow->show();
    connect(this, &MainWindow::on_showHistoryText, m_outputWindow, &TextOutputWindow::showHistory);

    connect(m_outputWindow, &TextOutputWindow::on_retranslate, this, [this] {
        m_tesseractOcr->clearCache();
    });

    connect(m_outputWindow, &TextOutputWindow::on_selectNewRegion, this, [this] {
        if (!m_overlayImage.isNull()) {
            m_overlayWindow->setInnerBrushActive(false);
            showOverlayWindow();
        }
    });

    connect(m_outputWindow, &TextOutputWindow::on_selectNewInnerRegion, this, [this] {
        if (!m_overlayWindow->getIsRectBrushEmpty()) {
            m_overlayWindow->setInnerBrushActive(true);
            showOverlayWindow();
        }
    });

    initHotKeys();
    initScreenCast();

    connect(ui->generalBoxLanguage, &QComboBox::currentIndexChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalToggledStartup, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged, this, &MainWindow::on_widgetChanged);
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
    connect(ui->textProcessingDelaySpinBox, &QDoubleSpinBox::valueChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->textProcessingTableWidget, &QTableWidget::currentItemChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->textProcessingLanguage, &QComboBox::currentTextChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->textProcessingSystemTessDataToggled, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->textProcessingTessdataPathLineEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyEnabledCheckBox, &QCheckBox::stateChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyAddressEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyPortEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyUserEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyPasswordEdit, &QLineEdit::textChanged, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyTypeHttp, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);
    connect(ui->proxyTypeSocks, &QRadioButton::toggled, this, &MainWindow::on_widgetChanged);

    connect(ui->translatorOnlineGoogleToggled, &QCheckBox::stateChanged, this, &MainWindow::on_translatorChanged);
    connect(ui->translatorOfflineOllamaToggled, &QCheckBox::stateChanged, this, &MainWindow::on_translatorChanged);

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
    if (m_ollama) { delete m_ollama; }
    if (m_google) { delete m_google; }
    if (m_outputWindow) { delete m_outputWindow; }
#ifdef Q_OS_LINUX
    if (m_portalScreencast) { delete m_portalScreencast; }
    if (m_portalHotKeys) { delete m_portalHotKeys; }
#endif
    if (m_captureRegionHotKey) { delete m_captureRegionHotKey; }
    if (m_showHistoryTranslationHotKey) { delete m_showHistoryTranslationHotKey; }

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
    } else if (role == QDialogButtonBox::RejectRole) {
        loadConfig();
    }
}

void MainWindow::on_portalShortcutActivated(const QString &shortcutId)
{
    if (shortcutId == "CaptureRegion") captureRegion();
    if (shortcutId == "HistoryTranslation") showHistory();
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
    ui->outputOriginalScreencast->clear();
    ui->outputProcessedScreencast->clear();
    m_overlayWindow->clearFrame();
    m_overlayImage = QImage();
}

void MainWindow::on_outputProcessedOtsu_stateChanged(int arg1)
{
    ui->outputProcessedThreshValue->setEnabled(!arg1);
}

void MainWindow::on_translatorOnlineGoogleSettingsButton_clicked()
{
    this->setEnabled(false);

    GoogleSettingsDialog *dialog = new GoogleSettingsDialog(m_googleSourceLang, m_googleTargetLang, m_google, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QEventLoop loop;
    connect(dialog, &QDialog::finished, &loop, &QEventLoop::quit);
    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if (result == QDialog::Accepted) {
            m_googleSourceLang = dialog->getSourceLang();
            m_googleTargetLang = dialog->getTargetLang();
            m_google->setSourceLang(m_googleSourceLang);
            m_google->setTargetLang(m_googleTargetLang);
            saveConfig();
        }
        this->setEnabled(true);
    });

    dialog->show();
    loop.exec();
}

void MainWindow::on_translatorOfflineOllamaToggled_stateChanged(int arg1)
{
    if (arg1 == Qt::Checked) {
        m_ollama->checkServerAvailable(m_ollamaUrl, [this](bool isAvailable) {
            if (!isAvailable) {
                ui->translatorOfflineOllamaToggled->setChecked(false);
                Log(Logger::Level::Warning, "[ollama] Server is unavailable");
                QMessageBox::warning(this, "Ollama", tr("Server is unavailable"));
            } else {
                Log(Logger::Level::Info, "[ollama] Server is available");

                m_ollama->checkModelsAvailable([this](QStringList models) {
                    if (models.isEmpty()) {
                        Log(Logger::Level::Warning, "[ollama] Failed to load models or list is empty");
                        return;
                    }
                    m_ollamaModels = models;
                });
            }
        });
    } else {
        m_ollamaModels.clear();
    }
}

void MainWindow::on_translatorOfflineOllamaSettingsButton_clicked()
{
    this->setEnabled(false);

    OllamaSettingsDialog *dialog = new OllamaSettingsDialog(m_ollamaUrl, m_ollamaCurrentModel, m_ollamaModels, m_ollamaPrompt, m_ollama, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QEventLoop loop;
    connect(dialog, &QDialog::finished, &loop, &QEventLoop::quit);
    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if (result == QDialog::Accepted) {
            m_ollamaUrl = dialog->getUrl();
            m_ollamaCurrentModel = dialog->getCurrentModel();
            m_ollamaPrompt = dialog->getPrompt();
            saveConfig();
        }
        this->setEnabled(true);
    });

    dialog->show();
    loop.exec();
}

void MainWindow::on_textProcessingDelaySpinBox_valueChanged(double arg1)
{
    m_tesseractOcr->setDelay(arg1);
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

    if (ui->listSettingsWidget->currentRow() == 1 && !frame.isNull()) {
        ui->outputOriginalScreencast->setPixmap(QPixmap::fromImage(frame).scaled(ui->outputOriginalScreencast->size(), Qt::KeepAspectRatio));
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
    if (ui->listSettingsWidget->currentRow() == 1 && !frame.isNull()) {
        ui->outputProcessedScreencast->setPixmap(QPixmap::fromImage(frame).scaled(ui->outputProcessedScreencast->size(), Qt::KeepAspectRatio));
    }
}

void MainWindow::setCurrentStatus(const QString &status)
{
    ui->textProcessingStatusTesseractLabel->setText(status);
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
        QJsonObject translator = Config::getValue("translator_offline").toJsonObject();
        QJsonObject ollama = translator["ollama"].toObject();
        m_ollama->generate(ollama["ollama_prompt"].toString() + outputFilt, m_ollamaCurrentModel, [this, outputFilt](QString result){
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

void MainWindow::on_textProcessingUpdateListLangButton_clicked()
{
    m_tesseractOcr->stop();
    ui->textProcessingLanguage->clear();
    m_tesseractCurrentLang.clear();
    m_tesseractOcr->setTessdataPath("");

    if (!ui->textProcessingSystemTessDataToggled->isChecked()) {
        QString tessdataPath = ui->textProcessingTessdataPathLineEdit->text();
        if (!QDir(tessdataPath).exists()) {
            Log(Logger::Level::Warning, "[tesseract] The specified Tesseract data directory does not exist or is invalid");
            QMessageBox::warning(this, tr("Invalid Tesseract Data Directory"),
                                 tr("The specified Tesseract data directory does not exist or is invalid.\n"
                                    "Please provide a valid path or try using the system default directory."));
            return;
        }
        m_tesseractOcr->setTessdataPath(tessdataPath);
    }

    std::vector<std::string> languages = m_tesseractOcr->checkAvailableLanguages();

    if (languages.empty()) {
        Log(Logger::Level::Warning, "[tesseract] Tesseract could not find any language data in system locations");
        QMessageBox::information(this, tr("No Tesseract available languages found"),
                                 tr("Tesseract could not find any language data in system locations.\n"
                                    "Please install Tesseract language packs or specify a custom 'tessdata' directory."));
        return;
    }

    for (const auto& language : languages) {
        ui->textProcessingLanguage->addItem(QString::fromStdString(language));
    }
}

void MainWindow::on_textProcessingSystemTessDataToggled_stateChanged(int arg1)
{
    bool enabled = (arg1 == Qt::Unchecked);

    ui->textProcessingTessdataPathLabel->setEnabled(enabled);
    ui->textProcessingTessdataPathLineEdit->setEnabled(enabled);
    ui->textProcessingTessdataPathButton->setEnabled(enabled);
}

void MainWindow::on_textProcessingTessdataPathButton_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(nullptr, tr("Select folder"));
    if (!folderPath.isEmpty()) {
        ui->textProcessingTessdataPathLineEdit->setText(folderPath);
    }
}

void MainWindow::initHotKeys()
{
    if (ui->generalRadioHotKey->isChecked()) {
        m_captureRegionHotKey = new HotKeys();
        m_captureRegionHotKey->setShortcut(ui->generalHotkeySelectNewRegionEdit->keySequence());
        connect(m_captureRegionHotKey, &HotKeys::activated, this, &MainWindow::captureRegion);
        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged, m_captureRegionHotKey, &HotKeys::setShortcut);
        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::editingFinished, this, [this] {
            ui->generalHotkeySelectNewRegionEdit->clearFocus();
        });

        m_showHistoryTranslationHotKey = new HotKeys();
        m_showHistoryTranslationHotKey->setShortcut(ui->generalHotkeyHistoryTranslationEdit->keySequence());
        connect(m_showHistoryTranslationHotKey, &HotKeys::activated, this, &MainWindow::showHistory);
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged, m_showHistoryTranslationHotKey, &HotKeys::setShortcut);
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::editingFinished, this, [this] {
            ui->generalHotkeyHistoryTranslationEdit->clearFocus();
        });
    }
#ifdef Q_OS_LINUX
    else {
        m_portalHotKeys = new PortalHotkeys();
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
    m_overlayWindow->setPixmap(QPixmap::fromImage(m_overlayImage.copy().scaled(primaryScreenGeometry.width(), primaryScreenGeometry.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
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
    connect(m_opencv, &OpenCV::currentProcessedMat, m_tesseractOcr, &TesseractOcr::frameMat);
    connect(m_tesseractOcr, &TesseractOcr::currentStatus, this, &MainWindow::setCurrentStatus);
    connect(m_tesseractOcr, &TesseractOcr::currentOutputOCR, this, &MainWindow::setCurrentOutputOCR);
    connect(this, &MainWindow::currentOverlayText, m_outputWindow, &TextOutputWindow::setCurrentOutputOCR);
    connect(this, &MainWindow::clearOverlayText, m_outputWindow, &TextOutputWindow::clearOverlayText);

    const QString language = ui->textProcessingLanguage->currentText();
    if (!language.isEmpty()) {
        m_tesseractOcr->init(language);
    }
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
        connect(m_screenCastWindow, &ScreenCastWindow::on_screencastWindowShown, this, [this] {
            connect(m_opencv, &OpenCV::currentOriginalFrame, m_screenCastWindow, &ScreenCastWindow::setCurrentOriginalFrame);
        });
        connect(m_screenCastWindow, &ScreenCastWindow::on_screencastWindowHidden, this, [this] {
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

#ifdef Q_OS_LINUX
    if (!general["hotkeys_type"].toString().isEmpty()) {
        m_initHotKeyMode = general["hotkeys_type"].toString();

        bool isX11Mode = (m_initHotKeyMode == "x11");
        ui->generalRadioHotKey->setChecked(isX11Mode);
        ui->generalRadioHotKeyPortal->setChecked(!isX11Mode);
        ui->generalHotkeySelectNewRegionEdit->setEnabled(isX11Mode);
        ui->generalHotkeyHistoryTranslationEdit->setEnabled(isX11Mode);
        ui->generalBindShortcut->setEnabled(!isX11Mode);
    }
#endif

    QJsonObject output = Config::getValue("output").toJsonObject();
    if (!output.empty()) {
        ui->outputToggledOriginalScreencast->setChecked(output["original_screencast_output"].toBool());
        ui->outputToggledProcessedScreencast->setChecked(output["processed_screencast_output"].toBool());
        ui->outputGeneralBoxFramerate->setCurrentIndex(output["framerate_index"].toInt());
    }

    QJsonObject processing = output["processing"].toObject();
    if (!processing.empty()) {
        ui->outputProcessedSimpleThresh->setChecked(processing["is_simple_thresholding"].toBool());
        ui->outputProcessedAdaptiveThresh->setChecked(processing["is_adaptive_thresholding"].toBool());
        ui->outputProcessedOtsu->setChecked(processing["is_otsu_binarization"].toBool());
        ui->outputProcessedSimpleThresholdingType->setCurrentIndex(processing["thresholding_type"].toInt());
        ui->outputProcessedThreshValue->setValue(processing["threshold_value"].toInt());
        ui->outputProcessedAdaptiveThresholdingType->setCurrentIndex(processing["adaptive_method"].toInt());
    }

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

    QJsonObject translator_online = Config::getValue("translator_online").toJsonObject();
    if (!translator_online.isEmpty()) {
        QJsonObject google = translator_online["google"].toObject();
        ui->translatorOnlineGoogleToggled->setChecked(google["is_google"].toBool());
        m_googleSourceLang = google["google_source_lang"].toString();
        m_googleTargetLang = google["google_target_lang"].toString();
        m_google->setSourceLang(m_googleSourceLang);
        m_google->setTargetLang(m_googleTargetLang);
    }

    QJsonObject translator_offline = Config::getValue("translator_offline").toJsonObject();
    if (!translator_offline.isEmpty()) {
        QJsonObject ollama = translator_offline["ollama"].toObject();
        ui->translatorOfflineOllamaToggled->setChecked(ollama["is_ollama"].toBool());
        QString ollamaUrl = ollama["ollama_url"].toString();
        if (ollamaUrl != "") {m_ollamaUrl = ollamaUrl; }
        m_ollamaCurrentModel = ollama["ollama_model"].toString();
        m_ollamaPrompt = ollama["ollama_prompt"].toString();
    }

    QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();
    if (!textProcessing.isEmpty()) {
        ui->textProcessingSystemTessDataToggled->setChecked(textProcessing["is_systemdata"].toBool());
        ui->textProcessingTessdataPathLineEdit->setText(textProcessing["path_tessdata"].toString());
        ui->textProcessingDelaySpinBox->setValue(textProcessing["delay"].toDouble());

        m_tesseractCurrentLang = textProcessing.value("tesseract_lang").toString();

        if (!m_tesseractOcr->isRunning() && !m_tesseractCurrentLang.isEmpty()) {
            bool tessdataPathValid = true;
            if (ui->textProcessingSystemTessDataToggled->isChecked()) {
                m_tesseractOcr->setTessdataPath(QString());
                QString tessdataPath = QString();
            } else {
                QString tessdataPath = ui->textProcessingTessdataPathLineEdit->text();
                QDir dir(tessdataPath);
                if (!dir.exists()) {
                    tessdataPathValid = false;
                } else {
                    m_tesseractOcr->setTessdataPath(tessdataPath);
                }
            }

            if (tessdataPathValid) {
                std::vector<std::string> languages = m_tesseractOcr->checkAvailableLanguages();
                ui->textProcessingLanguage->clear();
                for (const auto& language : languages) {
                    ui->textProcessingLanguage->addItem(QString::fromStdString(language));
                }
            }
        }

        int index = ui->textProcessingLanguage->findText(textProcessing.value("tesseract_lang").toString());
        if (index != -1) {
            ui->textProcessingLanguage->setCurrentIndex(index);
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

    if (!m_tesseractOcr->isRunning()) initTesseractOCR();

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
    Config::setValue("general", general);

    QJsonObject output;
    output["original_screencast_output"] = ui->outputToggledOriginalScreencast->isChecked();
    output["processed_screencast_output"] = ui->outputToggledProcessedScreencast->isChecked();
    output["framerate_index"] = ui->outputGeneralBoxFramerate->currentIndex();

    QJsonObject processing;
    processing["is_simple_thresholding"] = ui->outputProcessedSimpleThresh->isChecked();
    processing["is_adaptive_thresholding"] = ui->outputProcessedAdaptiveThresh->isChecked();
    processing["is_otsu_binarization"] = ui->outputProcessedOtsu->isChecked();
    processing["simple_threshold_type"] = ui->outputProcessedSimpleThresholdingType->currentIndex();
    processing["threshold_value"] = ui->outputProcessedThreshValue->value();
    processing["adaptive_method"] = ui->outputProcessedAdaptiveThresholdingType->currentIndex();

    output["processing"] = processing;
    Config::setValue("output", output);

    QJsonObject translator_online, google;
    google["is_google"] = ui->translatorOnlineGoogleToggled->isChecked();
    google["google_source_lang"] = m_googleSourceLang;
    google["google_target_lang"] = m_googleTargetLang;
    translator_online.insert("google", google);
    Config::setValue("translator_online", translator_online);

    QJsonObject translator_offline, ollama;
    ollama["is_ollama"] = ui->translatorOfflineOllamaToggled->isChecked();
    ollama["ollama_url"] = m_ollamaUrl;
    ollama["ollama_model"] = m_ollamaCurrentModel;
    ollama["ollama_prompt"] = m_ollamaPrompt;
    translator_offline.insert("ollama", ollama);
    Config::setValue("translator_offline", translator_offline);

    QJsonObject textProcessing;
    textProcessing["delay"] = ui->textProcessingDelaySpinBox->value();
    textProcessing["tesseract_lang"] = ui->textProcessingLanguage->currentText();
    textProcessing["is_systemdata"] = ui->textProcessingSystemTessDataToggled->isChecked();
    textProcessing["path_tessdata"] = ui->textProcessingTessdataPathLineEdit->text();

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

    Config::saveConfig("settings.json");

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

    if (ui->textProcessingLanguage->currentText() != m_tesseractCurrentLang)
    {
        m_tesseractCurrentLang = ui->textProcessingLanguage->currentText();
        m_tesseractOcr->stop();
        m_tesseractOcr->init(ui->textProcessingLanguage->currentText());
    }

#ifdef Q_OS_LINUX
    if (m_initHotKeyMode != general["hotkeys_type"].toString() || m_initLanguage != general["language"].toString()) {
#elif defined(Q_OS_WIN)
    if (m_initLanguage != general["language"].toString() && !general.isEmpty()) {
#endif
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Restart"));
        msgBox.setText(tr("A restart is required. Do you want to restart now?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

        int ret = msgBox.exec();
        switch (ret) {
        case QMessageBox::Yes:
            qApp->quit();
            QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
            break;
        case QMessageBox::No:
            break;
        default:
            break;
        }
    }
}
