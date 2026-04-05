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
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QTimer>
#include <QJsonArray>
#include <QProcess>
#include <QMenu>

#include "mainwindow.h"
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
    setupBaseUI();
    setupCoreConnections();
    loadApplicationConfig();
    initSubsystems();
    setupSettingsConnections();
    loadLogMessages();
    setupFinalUI();
}

MainWindow::~MainWindow()
{
    if (m_tesseractOcr) {
        m_tesseractOcr->stop();
        m_tesseractOcr->wait();
    }

    if (m_opencv) {
        m_opencv->setIsStopped(true);
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

    delete ui;
}

void MainWindow::setupBaseUI()
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

    m_changedWidgets = {
        ui->generalBoxLanguage,
        ui->generalToggledStartup,
        ui->generalHotkeySelectNewRegionEdit,
        ui->generalHotkeyHistoryTranslationEdit,
        ui->generalHotkeyManualTranslateEdit,
        ui->generalRadioHotKey,
        ui->generalRadioHotKeyPortal,
        ui->outputToggledOriginalScreencast,
        ui->outputToggledProcessedScreencast,
        ui->outputToggledScreencast,
        ui->outputGeneralBoxFramerate,
        ui->outputProcessedToggledBlur,
        ui->outputProcessedBlurType,
        ui->outputProcessedBlurValue,
        ui->outputProcessedBlurSubtract,
        ui->outputProcessedBlurNormalize,
        ui->outputProcessedSimpleThresh,
        ui->outputProcessedAdaptiveThresh,
        ui->outputProcessedOtsu,
        ui->outputProcessedSimpleThresholdingType,
        ui->outputProcessedThreshValue,
        ui->outputProcessedAdaptiveThresholdingType,
        ui->translatorOnlineGoogleToggled,
        ui->translatorOfflineOllamaToggled,
        ui->textProcessingOCREngineToggled,
        ui->textProcessingOCREngineTesseractRadio,
        ui->textProcessingOCREngineOllamaVisionRadio,
        ui->textProcessingTableWidget,
        ui->proxyEnabledCheckBox,
        ui->proxyAddressEdit,
        ui->proxyPortEdit,
        ui->proxyUserEdit,
        ui->proxyPasswordEdit,
        ui->proxyTypeHttp,
        ui->proxyTypeSocks
    };
}

void MainWindow::setupCoreConnections()
{
    // UI base
    connect(m_screen, &QScreen::availableGeometryChanged, this, &MainWindow::on_availableGeometryChanged);
    connect(ui->listSettingsWidget, &QListWidget::currentRowChanged, ui->settingsPages, &QStackedWidget::setCurrentIndex);

    // Blur
    connect(ui->outputProcessedToggledBlur, &QCheckBox::stateChanged, m_opencv, &OpenCV::setBlurEnabled);
    connect(ui->outputProcessedBlurType, &QComboBox::currentIndexChanged, m_opencv, &OpenCV::setBlurType);
    connect(ui->outputProcessedBlurValue, &QSlider::valueChanged, m_opencv, &OpenCV::setBlurSize);
    connect(ui->outputProcessedBlurSubtract, &QCheckBox::stateChanged, m_opencv, &OpenCV::setSubtractBlur);
    connect(ui->outputProcessedBlurNormalize, &QCheckBox::stateChanged, m_opencv, &OpenCV::setNormalizeBlur);

    // Threshold
    connect(ui->outputProcessedSimpleThresh, &QRadioButton::toggled, m_opencv, &OpenCV::setThresholdMethod);
    connect(ui->outputProcessedSimpleThresholdingType, &QComboBox::currentIndexChanged, m_opencv, &OpenCV::setSimpleThresholdType);
    connect(ui->outputProcessedThreshValue, &QSlider::valueChanged, m_opencv, &OpenCV::setThresholdValue);
    connect(ui->outputProcessedOtsu, &QCheckBox::stateChanged, m_opencv, &OpenCV::setOtsuEnabled);
    connect(ui->outputProcessedAdaptiveThresholdingType, &QComboBox::currentIndexChanged, m_opencv, &OpenCV::setAdaptiveThresholdType);
}

void MainWindow::loadApplicationConfig()
{
    setPropertyChanged(true);
    if (Config::isLoaded()) {
        loadConfig();
    } else {
        show();
        saveConfig();
    }
}

void MainWindow::initSubsystems()
{
    // Overlay Window
    m_overlayWindow->raise();
    connect(m_overlayWindow, &OverlayWindow::hideOverlay, this, [this] {
        m_overlayWindow->hide();
        m_outputWindow->show();
    });

    // Output Window
    m_outputWindow->show();
    connect(this, &MainWindow::showHistoryRequested, m_outputWindow, &TextOutputWindow::showHistory);
    connect(m_outputWindow, &TextOutputWindow::retranslateRequested, this, &MainWindow::retranslateText);
    connect(m_outputWindow, &TextOutputWindow::selectNewRegionRequested, this, &MainWindow::selectNewRegion);
    connect(m_outputWindow, &TextOutputWindow::selectNewInnerRegionRequested, this, &MainWindow::selectNewInnerRegion);

    // Ollama Settings
    m_ollamaSettingsDialog = new OllamaSettingsDialog(m_ollama, m_ollamaCurrentModel, m_ollamaModels, this);

    initHotKeys();
    initScreenCast();

    // Tesseract
    connect(m_tesseractOcr, &TesseractOcr::currentOutputOCR, this, &MainWindow::setCurrentOutput);
}

void MainWindow::setupSettingsConnections()
{
    auto bind = [this](bool& flag, QWidget* w) {
        return [this, &flag, w]() {
            flag = true;
            w->setProperty("changed", true);
            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        };
    };

    // General
    connect(ui->generalBoxLanguage, &QComboBox::currentIndexChanged, this, bind(m_generalChanged, ui->generalBoxLanguage));
    connect(ui->generalToggledStartup, &QCheckBox::stateChanged, this, bind(m_generalChanged, ui->generalToggledStartup));
    connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged, this, bind(m_generalChanged, ui->generalHotkeySelectNewRegionEdit));
    connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged, this, bind(m_generalChanged, ui->generalHotkeyHistoryTranslationEdit));
    connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::keySequenceChanged, this, bind(m_generalChanged, ui->generalHotkeyManualTranslateEdit));
    connect(ui->generalRadioHotKey, &QRadioButton::toggled, this, bind(m_generalChanged, ui->generalRadioHotKey));
    connect(ui->generalRadioHotKeyPortal, &QRadioButton::toggled, this, bind(m_generalChanged, ui->generalRadioHotKeyPortal));

    // Output
    connect(ui->outputToggledOriginalScreencast, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputToggledOriginalScreencast));
    connect(ui->outputToggledProcessedScreencast, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputToggledProcessedScreencast));
    connect(ui->outputToggledScreencast, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputToggledScreencast));
    connect(ui->outputGeneralBoxFramerate, &QComboBox::currentIndexChanged, this, bind(m_outputChanged, ui->outputGeneralBoxFramerate));
    connect(ui->outputProcessedToggledBlur, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputProcessedToggledBlur));
    connect(ui->outputProcessedBlurType, &QComboBox::currentIndexChanged, this, bind(m_outputChanged, ui->outputProcessedBlurType));
    connect(ui->outputProcessedBlurValue, &QSlider::valueChanged, this, bind(m_outputChanged, ui->outputProcessedBlurValue));
    connect(ui->outputProcessedBlurSubtract, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputProcessedBlurSubtract));
    connect(ui->outputProcessedBlurNormalize, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputProcessedBlurNormalize));
    connect(ui->outputProcessedSimpleThresh, &QRadioButton::toggled, this, bind(m_outputChanged, ui->outputProcessedSimpleThresh));
    connect(ui->outputProcessedAdaptiveThresh, &QRadioButton::toggled, this, bind(m_outputChanged, ui->outputProcessedAdaptiveThresh));
    connect(ui->outputProcessedOtsu, &QCheckBox::stateChanged, this, bind(m_outputChanged, ui->outputProcessedOtsu));
    connect(ui->outputProcessedSimpleThresholdingType, &QComboBox::currentIndexChanged, this, bind(m_outputChanged, ui->outputProcessedSimpleThresholdingType));
    connect(ui->outputProcessedThreshValue, &QSlider::valueChanged, this, bind(m_outputChanged, ui->outputProcessedThreshValue));
    connect(ui->outputProcessedAdaptiveThresholdingType, &QComboBox::currentIndexChanged, this, bind(m_outputChanged, ui->outputProcessedAdaptiveThresholdingType));

    // Translator
    connect(ui->translatorOnlineGoogleToggled, &QCheckBox::stateChanged, this, bind(m_translatorChanged, ui->translatorOnlineGoogleToggled));
    connect(ui->translatorOfflineOllamaToggled, &QCheckBox::stateChanged, this, bind(m_translatorChanged, ui->translatorOfflineOllamaToggled));

    // Text Processing
    connect(ui->textProcessingOCREngineToggled, &QCheckBox::stateChanged, this, bind(m_textProcessingChanged, ui->textProcessingOCREngineToggled));
    connect(ui->textProcessingOCREngineTesseractRadio, &QRadioButton::toggled, this, bind(m_textProcessingChanged, ui->textProcessingOCREngineTesseractRadio));
    connect(ui->textProcessingOCREngineOllamaVisionRadio, &QRadioButton::toggled, this, bind(m_textProcessingChanged, ui->textProcessingOCREngineOllamaVisionRadio));
    connect(ui->textProcessingTableWidget, &QTableWidget::currentItemChanged, this, bind(m_textProcessingChanged, ui->textProcessingTableWidget));

    // Proxy
    connect(ui->proxyEnabledCheckBox, &QCheckBox::stateChanged, this, bind(m_proxyChanged, ui->proxyEnabledCheckBox));
    connect(ui->proxyAddressEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyAddressEdit));
    connect(ui->proxyPortEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyPortEdit));
    connect(ui->proxyUserEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyUserEdit));
    connect(ui->proxyPasswordEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyPasswordEdit));
    connect(ui->proxyTypeHttp, &QRadioButton::toggled, this, bind(m_proxyChanged, ui->proxyTypeHttp));
    connect(ui->proxyTypeSocks, &QRadioButton::toggled, this, bind(m_proxyChanged, ui->proxyTypeSocks));
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

void MainWindow::setupFinalUI()
{
    contextMenus[ui->outputOriginalScreencast] =
        createMenu(tr("Open Original Screencast in New Window"), &MainWindow::openOriginalPreview);

    contextMenus[ui->outputProcessedScreencast] =
        createMenu(tr("Open Processed Screencast in New Window"), &MainWindow::openProcessedPreview);

    for (QLabel *label : contextMenus.keys()) {
        label->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(label, &QLabel::customContextMenuRequested,
                this, &MainWindow::showContextMenu);
    }
}

bool MainWindow::widgetChanged(QWidget *widget)
{
    return widget->property("changed").toBool();
}

void MainWindow::setPropertyChanged(const bool &value)
{
    m_generalChanged = value;
    m_outputChanged = value;
    m_textProcessingChanged = value;
    m_translatorChanged = value;
    m_proxyChanged = value;

    for (QObject *w : m_changedWidgets) {
        if (w) w->setProperty("changed", QVariant(value));
    }
}

QMenu* MainWindow::createMenu(const QString &title, void (MainWindow::*slot)())
{
    QMenu *menu = new QMenu(this);
    menu->addAction(title, this, slot);
    return menu;
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    if (HoverLabel *label = qobject_cast<HoverLabel*>(sender())) {
        if (QMenu *menu = contextMenus.value(label)) {
            label->setHoverState(false);
            menu->exec(label->mapToGlobal(pos));
        }
    }
}

PreviewWindow* MainWindow::createPreviewWindow(const QString &title, void (OpenCV::*frameSignal)(const QImage&))
{
    auto *preview = new PreviewWindow();
    preview->setWindowTitle(title);
    preview->resize(640, 480);
    preview->setMinimumSize(320, 240);
    preview->setAttribute(Qt::WA_DeleteOnClose);
    preview->show();

    connect(m_opencv, frameSignal, preview, &PreviewWindow::setCurrentFrame);
    connect(this, &MainWindow::screenCastFinished, preview, &PreviewWindow::clearFrame);

    return preview;
}

void MainWindow::openOriginalPreview()
{
    createPreviewWindow(tr("Original Screencast Preview"), &OpenCV::currentOriginalFrame);
}

void MainWindow::openProcessedPreview()
{
    createPreviewWindow(tr("Processed Screencast Preview"), &OpenCV::currentProcessedFrame);
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
    if (shortcutId == "ManualTranslate") retranslateText();
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
        stopScreenCapture();
        m_portalScreencast->reload();
        return;
    }
#endif
    m_screenCastWindow->show();
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

void MainWindow::on_outputToggledScreencast_stateChanged(int arg1)
{
    bool enabled = (arg1 == Qt::Checked);
    ui->outputGeneralSelect->setEnabled(!enabled);

    if (enabled) {
        stopScreenCapture();
    } else {
        startScreenCapture();
    }
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

void MainWindow::on_textProcessingOCREngineToggled_stateChanged(int arg1)
{
    ui->textProcessingOCREngineTesseractRadio->setEnabled(arg1);
    ui->textProcessingOCREngineOllamaVisionRadio->setEnabled(arg1);
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

            m_textProcessingChanged = true;
            ui->textProcessingOCREngineTesseractRadio->setProperty("changed", true);
            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
    });

    m_tesseractSettingsDialog->show();
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
    if (dir.exists()) {
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

#ifdef Q_OS_LINUX
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
    Log(Logger::Level::Info, "[pipewire] Source selected");
    m_opencv->setIsStopped(false);
    m_pipewire->init(nodeId);
    m_pipewire->start();
    m_pipewire->setIsStopped(false);
}
#endif

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

    if (m_ocrEngine == OcrEngine::Tesseract)
        m_tesseractOcr->frameMat(frame);
    else
        m_ollama->frameMat(frame);
}

void MainWindow::startScreenCapture()
{
    if (m_opencv) {
        m_opencv->setIsStopped(false);
    }
#ifdef Q_OS_LINUX
    if (m_portalScreencast) {
        m_portalScreencast->init(m_currentRestoreToken);
    } else {
#endif
        m_screenCapture->init();
        m_screenCapture->start();
#ifdef Q_OS_LINUX
    }
#endif
}

void MainWindow::stopScreenCapture()
{
    if (m_opencv) {
        m_opencv->setIsStopped(true);
        ui->outputOriginalScreencast->clear();
        ui->outputProcessedScreencast->clear();
        m_overlayWindow->clearFrame();
        m_overlayImage = QImage();
        emit screenCastFinished();
    }

#ifdef Q_OS_LINUX
    if (m_pipewire) {
        m_pipewire->stop();
    }

    if (m_portalScreencast) {
        m_portalScreencast->stop();
    }
#endif

    if (m_screenCastWindow) {
        m_screenCastWindow->hide();
    }

    if (!m_overlayWindow->isHidden()) {
        m_overlayWindow->hide();
        m_outputWindow->show();
    }

    if (m_screenCapture) {
        m_screenCapture->stop();
    }
}

void MainWindow::setCurrentOutput(const QString &source, const QString &output)
{
    QString outputFilt = replaceText(output);

    // Google
    if (ui->translatorOnlineGoogleToggled->isChecked()) {
        m_google->translateText(outputFilt, [this, source, outputFilt](QString result) {
            m_outputWindow->setTranslationResult(source, "Google", outputFilt, result);
        });
    }

    // Ollama
    if (ui->translatorOfflineOllamaToggled->isChecked()) {
        QJsonObject ollama_translator = Config::getValue("translator").toJsonObject()
                                                .value("translator_offline").toObject()
                                                .value("ollama").toObject();
            if (!ollama_translator.isEmpty()) {
                m_ollama->generate(ollama_translator["translation_prompt"].toString() + outputFilt, m_ollamaCurrentModel, [this, source, outputFilt](QString result) {
                m_outputWindow->setTranslationResult(source, "Ollama", outputFilt, result);
            });
        }
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

            m_translatorChanged = true; m_textProcessingChanged = true;
            ui->textProcessingOCREngineOllamaVisionRadio->setProperty("changed", true);
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
        setCurrentOutput("Ollama Vision", result);
    });
}

void MainWindow::retranslateText()
{
    if (!m_overlayWindow->getIsRectBrushEmpty() && m_ocrEnabled) {
        switch (m_ocrEngine) {
            case OcrEngine::Tesseract:
                m_tesseractOcr->clearCache();
                m_tesseractOcr->triggerManualOCR();
                break;
            case OcrEngine::OllamaVision:
                m_ollama->generateVision(m_ollamaVisionPrompt, m_ollamaCurrentModel, [this](QString result){
                    setCurrentOutput("Ollama Vision", result);
                });
            default:
                break;
        }
    }
}

void MainWindow::selectNewRegion()
{
    if (!m_overlayImage.isNull()) {
        m_overlayWindow->setInnerBrushActive(false);
        showOverlayWindow();
    }
}
void MainWindow::selectNewInnerRegion()
{
    if (!m_overlayImage.isNull() && !m_overlayWindow->getIsRectBrushEmpty()) {
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
        connect(m_manualTranslateHotKey, &HotKeys::activated, this, &MainWindow::retranslateText);
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

        emit showHistoryRequested();
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
        QTableWidgetItem* fromItem = ui->textProcessingTableWidget->item(i, 0);
        QTableWidgetItem* toItem = ui->textProcessingTableWidget->item(i, 1);

        if (fromItem && toItem) {
            QString from = fromItem->text();
            QString to = toItem->text();
            output.replace(from, to);
        }
    }

    return output;
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
    m_portalScreencast = new ScreenCastPortal();
    connect(m_portalScreencast, &ScreenCastPortal::currentRestoreToken, this, &MainWindow::setCurrentRestoreToken);
    connect(m_portalScreencast, &ScreenCastPortal::currentNodeId, this, &MainWindow::setCurrentNodeId);

    connect(m_portalScreencast, &ScreenCastPortal::failedPortal, this, [this]
    {
        delete m_portalScreencast; m_portalScreencast = nullptr;
        delete m_pipewire; m_pipewire = nullptr;
#endif
        m_screenCapture = new ScreenCast();
        m_screenCapture->init();
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

        if (!ui->outputToggledScreencast->isChecked()) m_screenCapture->start();
#ifdef Q_OS_LINUX
    });

    if (!ui->outputToggledScreencast->isChecked()) m_portalScreencast->init(m_currentRestoreToken);
#endif
}

void MainWindow::loadConfig()
{
    if (m_generalChanged)
        loadGeneralSettings(Config::getValue("general").toJsonObject());
    if (m_outputChanged)
        loadOutputSettings(Config::getValue("output").toJsonObject());
    if (m_translatorChanged)
        loadTranslatorSettings(Config::getValue("translator").toJsonObject());
    if (m_textProcessingChanged)
        loadTextProcessingSettings(Config::getValue("text_processing").toJsonObject());
    if (m_proxyChanged)
        loadProxySettings(Config::getValue("proxy").toJsonObject());

    loadScreencastSettings();
    loadOcrSettings();

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    setPropertyChanged(false);
}

void MainWindow::loadGeneralSettings(const QJsonObject& general)
{
    if (widgetChanged(ui->generalBoxLanguage)) {
        m_initLanguage = general["language"].toString();
        int index = ui->generalBoxLanguage->findData(m_initLanguage);
        if (index != -1) {
            ui->generalBoxLanguage->setCurrentIndex(index);
        }
    }

    if (widgetChanged(ui->generalToggledStartup)) {
        if (!general["settings_startup"].isNull()) {
            bool hideStartup = general["settings_startup"].toBool();
            ui->generalToggledStartup->setChecked(hideStartup);

            if (!hideStartup) {
                show();
            }
        } else {
            show();
        }
    }

    if (widgetChanged(ui->generalHotkeySelectNewRegionEdit))
        ui->generalHotkeySelectNewRegionEdit->setKeySequence(QKeySequence(general["hotkey_select_region"].toString()));
    if (widgetChanged(ui->generalHotkeyHistoryTranslationEdit))
        ui->generalHotkeyHistoryTranslationEdit->setKeySequence(QKeySequence(general["hotkey_history_translation"].toString()));
    if (widgetChanged(ui->generalHotkeyManualTranslateEdit))
        ui->generalHotkeyManualTranslateEdit->setKeySequence(QKeySequence(general["hotkey_manual_translate"].toString()));

#ifdef Q_OS_LINUX
    if (!general["hotkeys_type"].toString().isEmpty()) {
        m_initHotKeyMode = general["hotkeys_type"].toString();

        bool isX11Mode = (m_initHotKeyMode == "x11");
        if (widgetChanged(ui->generalRadioHotKey))
            ui->generalRadioHotKey->setChecked(isX11Mode);
        if (widgetChanged(ui->generalRadioHotKeyPortal))
            ui->generalRadioHotKeyPortal->setChecked(!isX11Mode);
        if (widgetChanged(ui->generalHotkeySelectNewRegionEdit))
            ui->generalHotkeySelectNewRegionEdit->setEnabled(isX11Mode);
        if (widgetChanged(ui->generalHotkeyHistoryTranslationEdit))
            ui->generalHotkeyHistoryTranslationEdit->setEnabled(isX11Mode);
        if (widgetChanged(ui->generalHotkeyManualTranslateEdit))
            ui->generalHotkeyManualTranslateEdit->setEnabled(isX11Mode);

        ui->generalBindShortcut->setEnabled(!isX11Mode);
    }
#endif
}

void MainWindow::loadOutputSettings(const QJsonObject& output)
{
    if (!output.empty()) {
        if (widgetChanged(ui->outputToggledOriginalScreencast))
            ui->outputToggledOriginalScreencast->setChecked(output["original_screencast_output"].toBool());
        if (widgetChanged(ui->outputToggledProcessedScreencast))
            ui->outputToggledProcessedScreencast->setChecked(output["processed_screencast_output"].toBool());
        if (widgetChanged(ui->outputToggledScreencast))
            ui->outputToggledScreencast->setChecked(output["disable_screencast"].toBool());
        if (widgetChanged(ui->outputGeneralBoxFramerate))
            ui->outputGeneralBoxFramerate->setCurrentIndex(output["framerate_index"].toInt());
    }

    QJsonObject processing = output["processing"].toObject();
    if (!processing.empty()) {
        if (widgetChanged(ui->outputProcessedToggledBlur))
            ui->outputProcessedToggledBlur->setChecked(processing["is_blur"].toBool());
        if (widgetChanged(ui->outputProcessedBlurType))
            ui->outputProcessedBlurType->setCurrentIndex(processing["blur_type"].toInt());
        if (widgetChanged(ui->outputProcessedBlurValue))
            ui->outputProcessedBlurValue->setValue(processing["blur_value"].toInt());
        if (widgetChanged(ui->outputProcessedBlurSubtract))
            ui->outputProcessedBlurSubtract->setChecked(processing["is_blurSubtract"].toBool());
        if (widgetChanged(ui->outputProcessedBlurNormalize))
            ui->outputProcessedBlurNormalize->setChecked(processing["is_blurNormalize"].toBool());
        if (widgetChanged(ui->outputProcessedSimpleThresh))
            ui->outputProcessedSimpleThresh->setChecked(processing["is_simple_thresholding"].toBool());
        if (widgetChanged(ui->outputProcessedAdaptiveThresh))
            ui->outputProcessedAdaptiveThresh->setChecked(processing["is_adaptive_thresholding"].toBool());
        if (widgetChanged(ui->outputProcessedOtsu))
            ui->outputProcessedOtsu->setChecked(processing["is_otsu_binarization"].toBool());
        if (widgetChanged(ui->outputProcessedSimpleThresholdingType))
            ui->outputProcessedSimpleThresholdingType->setCurrentIndex(processing["simple_threshold_type"].toInt());
        if (widgetChanged(ui->outputProcessedThreshValue))
            ui->outputProcessedThreshValue->setValue(processing["threshold_value"].toInt());
        if (widgetChanged(ui->outputProcessedAdaptiveThresholdingType))
            ui->outputProcessedAdaptiveThresholdingType->setCurrentIndex(processing["adaptive_method"].toInt());
    }
}

void MainWindow::loadTranslatorSettings(const QJsonObject& translator)
{
    QJsonObject translator_online = translator["translator_online"].toObject();
    if (!translator_online.isEmpty()) {
        QJsonObject google = translator_online["google"].toObject();
        if (widgetChanged(ui->translatorOnlineGoogleToggled))
            ui->translatorOnlineGoogleToggled->setChecked(google["is_google"].toBool());

        m_googleSourceLang = google["google_source_lang"].toString();
        m_googleTargetLang = google["google_target_lang"].toString();
        m_google->setSourceLang(m_googleSourceLang);
        m_google->setTargetLang(m_googleTargetLang);
    }

    if (!ui->translatorOnlineGoogleToggled->isChecked() && widgetChanged(ui->translatorOnlineGoogleToggled)) {
        m_outputWindow->clearResultsByTranslator("Google");
    }

    QJsonObject translator_offline = translator["translator_offline"].toObject();
    if (!translator_offline.isEmpty()) {
        QJsonObject ollama_translator = translator_offline["ollama"].toObject();
        if (widgetChanged(ui->translatorOfflineOllamaToggled))
            ui->translatorOfflineOllamaToggled->setChecked(ollama_translator["is_ollama_translator"].toBool());

        QString ollamaUrl = ollama_translator["url"].toString();
        if (ollamaUrl != "") {m_ollamaUrl = ollamaUrl; m_ollama->setUrl(m_ollamaUrl); }
        m_ollamaCurrentModel = ollama_translator["current_model"].toString();
        m_ollamaModels = ollama_translator["models"].toArray();
        m_ollamaTranslationPrompt = ollama_translator["translation_prompt"].toString();
        m_waitForOllamaResponse = ollama_translator["wait_for_responce"].toBool();
    }

    if (!ui->translatorOfflineOllamaToggled->isChecked() && widgetChanged(ui->translatorOfflineOllamaToggled)) {
        m_outputWindow->clearResultsByTranslator("Ollama");
    }
}

void MainWindow::loadTextProcessingSettings(const QJsonObject& textProcessing)
{
    if (!textProcessing.isEmpty()) {

        // OCR
        if (widgetChanged(ui->textProcessingOCREngineToggled))
            ui->textProcessingOCREngineToggled->setChecked(textProcessing["is_ocr"].toBool());

        // Tesseract
        QJsonObject tesseract = textProcessing["tesseract"].toObject();
        if (widgetChanged(ui->textProcessingOCREngineTesseractRadio))
            ui->textProcessingOCREngineTesseractRadio->setChecked(tesseract["is_tesseract"].toBool());

        m_tesseractActiveLang = tesseract.value("lang").toString();
        m_tesseractUseSystemTessdata = tesseract["is_systemdata"].toBool();
        m_tesseractTessdataPath = tesseract["path_tessdata"].toString();
        m_tesseractMode = tesseract["mode"].toInt();
        m_tesseractAutoInterval = tesseract["delay"].toDouble();
        m_tesseractSelectedLang = m_tesseractActiveLang;

        m_tesseractOcr->setMode(m_tesseractMode);
        m_tesseractOcr->setDelay(m_tesseractAutoInterval);

        // Ollama Vision
        QJsonObject ollama_vision = textProcessing["ollama_vision"].toObject();
        if (widgetChanged(ui->textProcessingOCREngineOllamaVisionRadio))
            ui->textProcessingOCREngineOllamaVisionRadio->setChecked(ollama_vision["is_vision"].toBool());

        m_ollamaVisionPrompt = ollama_vision["prompt"].toString();
        m_ollamaVisionMode = ollama_vision["mode"].toInt(Manual);
        m_ollamaVisionAutoInterval = ollama_vision["delay"].toInt(10);

        // Text Replacement
        QJsonArray jsonArray = textProcessing["text_replacement_table"].toArray();
        if (widgetChanged(ui->textProcessingTableWidget)) {
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
    }
}

void MainWindow::loadProxySettings(const QJsonObject& proxy)
{
    if (!proxy.isEmpty()) {
        if (widgetChanged(ui->proxyEnabledCheckBox))
            ui->proxyEnabledCheckBox->setChecked(proxy["is_proxy"].toBool());
        if (widgetChanged(ui->proxyAddressEdit))
            ui->proxyAddressEdit->setText(proxy["ip"].toString());
        if (widgetChanged(ui->proxyPortEdit))
            ui->proxyPortEdit->setText(proxy["port"].toString());
        if (widgetChanged(ui->proxyUserEdit))
            ui->proxyUserEdit->setText(proxy["user"].toString());
        if (widgetChanged(ui->proxyPasswordEdit))
            ui->proxyPasswordEdit->setText(proxy["password"].toString());
        if (proxy["type"] == "http") {
            if (widgetChanged(ui->proxyTypeHttp))
                ui->proxyTypeHttp->setChecked(true);
        } else if (proxy["type"] == "socks") {
            if (widgetChanged(ui->proxyTypeSocks))
                ui->proxyTypeSocks->setChecked(true);
        }
    }

    if (ui->proxyEnabledCheckBox->isChecked() && widgetChanged(ui->proxyEnabledCheckBox))
    {
        QString ip = ui->proxyAddressEdit->text();
        QString port = ui->proxyPortEdit->text();
        QString user = ui->proxyUserEdit->text();
        QString password = ui->proxyPasswordEdit->text();

        QNetworkProxy proxy;
        proxy.setType(m_proxyType);
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
}

void MainWindow::loadScreencastSettings()
{
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
}

void MainWindow::loadOcrSettings()
{
    bool toggledChanged = widgetChanged(ui->textProcessingOCREngineToggled);
    bool isEnabled = ui->textProcessingOCREngineToggled->isChecked();

    if (!isEnabled && toggledChanged) {
        stopAllOcrEngines();
        m_outputWindow->clearResultsBySource("Tesseract");
        m_outputWindow->clearResultsBySource("Ollama Vision");
        return;
    }

    if (isEnabled && toggledChanged) {
        ui->textProcessingOCREngineTesseractRadio->setProperty("changed", true);
        ui->textProcessingOCREngineOllamaVisionRadio->setProperty("changed", true);
    }

    bool tesseractChanged = widgetChanged(ui->textProcessingOCREngineTesseractRadio);
    bool ollamaChanged = widgetChanged(ui->textProcessingOCREngineOllamaVisionRadio);

    if (!tesseractChanged && !ollamaChanged) return;

    m_ocrEngine = ui->textProcessingOCREngineTesseractRadio->isChecked()
    ? OcrEngine::Tesseract
    : OcrEngine::OllamaVision;

    if (!isEnabled) return;

    if (m_ocrEngine == OcrEngine::Tesseract && tesseractChanged) {
        stopOllamaVisionEngine();
        configureTesseractEngine();
    } else if (m_ocrEngine == OcrEngine::OllamaVision && ollamaChanged) {
        stopTesseractEngine();
        configureOllamaVisionEngine();
    }
}

void MainWindow::stopAllOcrEngines()
{
    if (m_tesseractOcr && m_tesseractOcr->isRunning()) {
        m_tesseractOcr->stop();
    }
    if (m_ollamaVisionTimer) {
        m_ollamaVisionTimer->stop();
        delete m_ollamaVisionTimer;
        m_ollamaVisionTimer = nullptr;
    }
}

void MainWindow::stopTesseractEngine()
{
    if (m_tesseractOcr && m_tesseractOcr->isRunning()) {
        m_tesseractOcr->stop();
    }
}

void MainWindow::stopOllamaVisionEngine()
{
    if (m_ollamaVisionTimer) {
        m_ollamaVisionTimer->stop();
        delete m_ollamaVisionTimer;
        m_ollamaVisionTimer = nullptr;
    }
}

void MainWindow::configureTesseractEngine()
{
    stopTesseractEngine();

    m_outputWindow->clearResultsBySource("Ollama Vision");

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

void MainWindow::configureOllamaVisionEngine()
{
    m_outputWindow->clearResultsBySource("Tesseract");

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
}

void MainWindow::saveConfig()
{
    // General
    if (m_generalChanged) {
        QJsonObject general = Config::getValue("general").toJsonObject();
        if (widgetChanged(ui->generalBoxLanguage))
            general["language"] = ui->generalBoxLanguage->currentData().toString();
        if (widgetChanged(ui->generalToggledStartup))
            general["settings_startup"] = ui->generalToggledStartup->isChecked();

    #ifdef Q_OS_LINUX
        if (ui->generalRadioHotKey->isChecked() && widgetChanged(ui->generalRadioHotKey)) {
            general["hotkeys_type"] = "x11";
        } else if (!ui->generalRadioHotKey->isChecked() && widgetChanged(ui->generalRadioHotKey)) {
            general["hotkeys_type"] = "portal";
        }
    #endif
        if (widgetChanged(ui->generalHotkeySelectNewRegionEdit))
            general["hotkey_select_region"] = ui->generalHotkeySelectNewRegionEdit->keySequence().toString();
        if (widgetChanged(ui->generalHotkeyHistoryTranslationEdit))
            general["hotkey_history_translation"] = ui->generalHotkeyHistoryTranslationEdit->keySequence().toString();
        if (widgetChanged(ui->generalHotkeyManualTranslateEdit))
            general["hotkey_manual_translate"] = ui->generalHotkeyManualTranslateEdit->keySequence().toString();
        Config::setValue("general", general);

#ifdef Q_OS_LINUX
        QString hotkeys = general.contains("hotkeys_type") ? general.value("hotkeys_type").toString() : QString();
        QString lang    = general.contains("language")     ? general.value("language").toString()     : QString();

        if ((!hotkeys.isEmpty() && m_initHotKeyMode != hotkeys)
            || (!lang.isEmpty() && m_initLanguage != lang)) {
#elif defined(Q_OS_WIN)
        QString lang = general.contains("language") ? general.value("language").toString() : QString();

        if (!lang.isEmpty() && m_initLanguage != lang) {
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

    // Output
    if (m_outputChanged) {
        QJsonObject output = Config::getValue("output").toJsonObject();
        if (widgetChanged(ui->outputToggledOriginalScreencast))
            output["original_screencast_output"] = ui->outputToggledOriginalScreencast->isChecked();
        if (widgetChanged(ui->outputToggledProcessedScreencast))
            output["processed_screencast_output"] = ui->outputToggledProcessedScreencast->isChecked();
        if (widgetChanged(ui->outputToggledScreencast))
            output["disable_screencast"] = ui->outputToggledScreencast->isChecked();
        if (widgetChanged(ui->outputGeneralBoxFramerate))
            output["framerate_index"] = ui->outputGeneralBoxFramerate->currentIndex();

        // Processing
        QJsonObject processing = output.value("processing").toObject();
        if (widgetChanged(ui->outputProcessedToggledBlur))
            processing["is_blur"] = ui->outputProcessedToggledBlur->isChecked();
        if (widgetChanged(ui->outputProcessedBlurType))
            processing["blur_type"] = ui->outputProcessedBlurType->currentIndex();
        if (widgetChanged(ui->outputProcessedBlurValue))
            processing["blur_value"] = ui->outputProcessedBlurValue->value();
        if (widgetChanged(ui->outputProcessedBlurSubtract))
            processing["is_blurSubtract"] = ui->outputProcessedBlurSubtract->isChecked();
        if (widgetChanged(ui->outputProcessedBlurNormalize))
            processing["is_blurNormalize"] = ui->outputProcessedBlurNormalize->isChecked();
        if (widgetChanged(ui->outputProcessedSimpleThresh))
            processing["is_simple_thresholding"] = ui->outputProcessedSimpleThresh->isChecked();
        if (widgetChanged(ui->outputProcessedAdaptiveThresh))
            processing["is_adaptive_thresholding"] = ui->outputProcessedAdaptiveThresh->isChecked();
        if (widgetChanged(ui->outputProcessedOtsu))
            processing["is_otsu_binarization"] = ui->outputProcessedOtsu->isChecked();
        if (widgetChanged(ui->outputProcessedSimpleThresholdingType))
            processing["simple_threshold_type"] = ui->outputProcessedSimpleThresholdingType->currentIndex();
        if (widgetChanged(ui->outputProcessedThreshValue))
            processing["threshold_value"] = ui->outputProcessedThreshValue->value();
        if (widgetChanged(ui->outputProcessedAdaptiveThresholdingType))
            processing["adaptive_method"] = ui->outputProcessedAdaptiveThresholdingType->currentIndex();

        output["processing"] = processing;
        Config::setValue("output", output);
    }

    // Translator
    if (m_translatorChanged) {
        QJsonObject translator = Config::getValue("translator").toJsonObject();
        QJsonObject translator_online = translator.value("translator_online").toObject();
        QJsonObject google = translator_online.value("google").toObject();

        if (widgetChanged(ui->translatorOnlineGoogleToggled))
            google["is_google"] = ui->translatorOnlineGoogleToggled->isChecked();

        google["google_source_lang"] = m_googleSourceLang;
        google["google_target_lang"] = m_googleTargetLang;

        translator_online.insert("google", google);
        translator.insert("translator_online", translator_online);

        QJsonObject translator_offline = Config::getValue("translator_offline").toJsonObject();
        QJsonObject ollama = translator_offline.value("ollama").toObject();

        if (widgetChanged(ui->translatorOfflineOllamaToggled))
            ollama["is_ollama_translator"] = ui->translatorOfflineOllamaToggled->isChecked();

        ollama["url"] = m_ollamaUrl;
        ollama["current_model"] = m_ollamaCurrentModel;
        ollama["models"] = m_ollamaModels;
        ollama["translation_prompt"] = m_ollamaTranslationPrompt;
        ollama["wait_for_responce"] = m_waitForOllamaResponse;
        translator_offline.insert("ollama", ollama);

        translator.insert("translator_offline", translator_offline);
        Config::setValue("translator", translator);
    }

    // Text Processing
    if (m_textProcessingChanged) {
        QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();

        // Tesseract
        QJsonObject tesseract = textProcessing.value("tesseract").toObject();
        if (widgetChanged(ui->textProcessingOCREngineTesseractRadio))
            tesseract["is_tesseract"] = ui->textProcessingOCREngineTesseractRadio->isChecked();

        tesseract["lang"] = m_tesseractSelectedLang;
        tesseract["is_systemdata"] = m_tesseractUseSystemTessdata;
        tesseract["path_tessdata"] = m_tesseractTessdataPath;
        tesseract["mode"] = m_tesseractMode;
        tesseract["delay"] = m_tesseractAutoInterval;

        // Ollama Vision
        QJsonObject ollama_vision = textProcessing.value("ollama_vision").toObject();
        if (widgetChanged(ui->textProcessingOCREngineOllamaVisionRadio))
            ollama_vision["is_vision"] = ui->textProcessingOCREngineOllamaVisionRadio->isChecked();

        ollama_vision["prompt"] = m_ollamaVisionPrompt;
        ollama_vision["mode"] = m_ollamaVisionMode;
        ollama_vision["delay"] = m_ollamaVisionAutoInterval;

        if (widgetChanged(ui->textProcessingOCREngineToggled))
            textProcessing["is_ocr"] = ui->textProcessingOCREngineToggled->isChecked();

        textProcessing.insert("tesseract", tesseract);
        textProcessing.insert("ollama_vision", ollama_vision);

        if (widgetChanged(ui->textProcessingTableWidget)) {
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
        }
        Config::setValue("text_processing", textProcessing);
    }

    // Proxy
    if (m_proxyChanged) {
        QJsonObject proxy = Config::getValue("proxy").toJsonObject();
        if (widgetChanged(ui->proxyEnabledCheckBox))
            proxy["is_proxy"] = ui->proxyEnabledCheckBox->isChecked();
        if (widgetChanged(ui->proxyAddressEdit))
            proxy["ip"] = ui->proxyAddressEdit->text();
        if (widgetChanged(ui->proxyPortEdit))
            proxy["port"] = ui->proxyPortEdit->text();
        if (widgetChanged(ui->proxyUserEdit))
            proxy["user"] = ui->proxyUserEdit->text();
        if (widgetChanged(ui->proxyPasswordEdit))
            proxy["password"] = ui->proxyPasswordEdit->text();

        if (widgetChanged(ui->proxyTypeHttp) && ui->proxyTypeHttp->isChecked()) {
            proxy["type"] = "http";
            m_proxyType = QNetworkProxy::HttpProxy;
        } else if (widgetChanged(ui->proxyTypeSocks) && ui->proxyTypeSocks->isChecked()) {
            proxy["type"] = "socks";
            m_proxyType = QNetworkProxy::Socks5Proxy;
        }
        Config::setValue("proxy", proxy);
    }

    // Save Config
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    Config::saveConfig("settings.json");
}
