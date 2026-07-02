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
#include <QJsonArray>
#include <QMenu>

#include "src/controllers/clipboardcontroller.h"

#include "mainwindow.h"
#include "textprocessingdelegates.h"
#include "ui_mainwindow.h"
#include "googlesettingsdialog.h"
#include "screencastwindow.h"
#include "src/controllers/translationcontroller.h"
#include "src/controllers/hotkeycontroller.h"
#include "src/controllers/capturecontroller.h"
#include "src/controllers/ocrcontroller.h"
#include "src/controllers/hookcontroller.h"
#include "src/utils/plugininterface.h"
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
    , m_opencv(new OpenCV(this))
    , m_pluginManager(new PluginManager(this))
    , m_translationController(new TranslationController(m_manager, this))
    , m_hotkeyController(new HotkeyController(this))
    , m_captureController(new CaptureController(this))
    , m_ocrController(new OcrController(m_manager, this))
    , m_hookController(new HookController(this))
{
    setupBaseUI();
    initPlugins();
    setupCoreConnections();
    loadApplicationConfig();
    initSubsystems();
    initClipboardController();
    setupSettingsConnections();

    connect(m_translationController, &TranslationController::translationReady,
            m_outputWindow, &TextOutputWindow::setTranslationResult);

    loadLogMessages();
    setupFinalUI();
}

MainWindow::~MainWindow()
{
    if (m_opencv) {
        m_opencv->setIsStopped(true);
    }

    if (m_captureController) {
        m_captureController->stop();
    }

    if (m_hookController) {
        m_hookController->stop();
    }

    if (m_overlayWindow) { delete m_overlayWindow; }
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
        ui->textProcessingHookCheckBox,
        ui->textProcessingClipboardCheckBox,
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

void MainWindow::initPlugins()
{
    m_registry = m_pluginManager->scanPlugins();
    m_pluginManager->loadPlugins();
    m_hookController->setRegistry(m_registry);

    ui->textProcessingHookRow->setVisible(false);

    QMap<QString, QStringList> dependencyErrors = m_pluginManager->validateDependencies(m_registry);

    ui->pluginsTableWidget->setRowCount(m_registry.size());
    int row = 0;
    for (const auto &p : m_registry) {
        bool hasErrors = dependencyErrors.contains(p.name);
        QStringList missingList = hasErrors ? dependencyErrors[p.name] : QStringList();

        QString depsText = p.dependencies.join(", ");

        QStringList archs = p.archPaths.keys();
        archs.sort();
        QTableWidgetItem* archItem = new QTableWidgetItem(archs.join(" | "));

        QTableWidgetItem *depsItem = new QTableWidgetItem(depsText);
        if (hasErrors) depsItem->setForeground(QBrush(Qt::red));

        ui->pluginsTableWidget->setItem(row, 0, new QTableWidgetItem(p.name));
        ui->pluginsTableWidget->setItem(row, 1, new QTableWidgetItem(p.version));
        ui->pluginsTableWidget->setItem(row, 2, archItem);
        ui->pluginsTableWidget->setItem(row, 3, depsItem);
        ui->pluginsTableWidget->setItem(row, 4, new QTableWidgetItem(p.description));
        ++row;

        if (p.type == "hook" && (p.category == "game" || p.category == "application")) {
            m_hookGameAppPluginList.insert(p.name, p.targetTitle);
        }

        if (p.type == "hook" && p.category == "engine") {
            m_hookEnginePluginList.insert(p.name, p.targetTitle);
        }

        if (p.name == "libat-injector" && !hasErrors) {
            ui->textProcessingHookCheckBox->setEnabled(true);
            ui->textProcessingHookRow->setVisible(true);

            if (!m_hookController->isPluginLoaded()) {
                QObject* pluginObj = m_pluginManager->getPlugin("libat-injector");
                PluginInterface *hookPlugin = qobject_cast<PluginInterface*>(pluginObj);

                if (!hookPlugin) {
                    Log(Logger::Level::Warning, "[Hook] Failed to load plugin 'libat-injector'");
                    m_outputWindow->setInfoMessage(tr("[Hook] Failed to load plugin 'libat-injector"));
                    ui->textProcessingHookCheckBox->setChecked(false);
                    saveConfig();
                    return;
                }

                m_hookController->setPlugin(hookPlugin);
            }
        }

        if (hasErrors) {
            Log(Logger::Level::Warning, QString("[plugin-loader] Plugin '%1' is invalid. Missing dependencies: %2")
                                            .arg(p.name, missingList.join(", ")));
            ui->textProcessingHookCheckBox->setEnabled(false);
        }
    }
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
    connect(m_outputWindow, &TextOutputWindow::manualInjectHookRequested, this, &MainWindow::manualInjectHook);

    // Ollama Settings
    m_ollamaSettingsDialog = new OllamaSettingsDialog(m_ocrController->ollama(), m_ollamaCurrentModel, m_ollamaModels, this);

    const HotkeyController::Mode hkMode =
#ifdef Q_OS_LINUX
        ui->generalRadioHotKey->isChecked() ? HotkeyController::X11 : HotkeyController::Portal;
#else
        HotkeyController::X11;
#endif
    m_hotkeyController->initialize(hkMode);

    if (hkMode == HotkeyController::X11) {
        m_hotkeyController->setCaptureRegionShortcut(ui->generalHotkeySelectNewRegionEdit->keySequence());
        m_hotkeyController->setShowHistoryShortcut(ui->generalHotkeyHistoryTranslationEdit->keySequence());
        m_hotkeyController->setRetranslateShortcut(ui->generalHotkeyManualTranslateEdit->keySequence());

        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setCaptureRegionShortcut);
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setShowHistoryShortcut);
        connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setRetranslateShortcut);

        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeySelectNewRegionEdit->clearFocus(); });
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeyHistoryTranslationEdit->clearFocus(); });
        connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeyManualTranslateEdit->clearFocus(); });
    }

    connect(m_hotkeyController, &HotkeyController::captureRegionTriggered,
            this, &MainWindow::captureRegion);
    connect(m_hotkeyController, &HotkeyController::showHistoryTriggered,
            this, &MainWindow::showHistory);
    connect(m_hotkeyController, &HotkeyController::retranslateTriggered,
            this, &MainWindow::retranslateText);
    connect(m_hotkeyController, &HotkeyController::shortcutReleased, this, [this] {
        if (m_isShortcuts) m_isShortcuts = false;
    });

    initScreenCast();

    connect(m_ocrController, &OcrController::textRecognized,
            this, &MainWindow::setCurrentOutput);

    connect(m_ocrController, &OcrController::engineDeactivated, m_outputWindow,
            [this](const QString &source) {
                m_outputWindow->clearResultsBySource(source);
            });

    connect(m_hookController, &HookController::textReceived,
            this, &MainWindow::setCurrentOutput);
    connect(m_hookController, &HookController::infoMessage,
            m_outputWindow, &TextOutputWindow::setInfoMessage);
    connect(m_hookController, &HookController::hookStateChanged,
            m_outputWindow, &TextOutputWindow::sethookState);
    connect(m_hookController, &HookController::shouldClearResults, m_outputWindow, [this] {
        m_outputWindow->clearResultsBySource(QStringLiteral("Hook"));
    });
    connect(m_hookController, &HookController::shouldClearInfoMessage,
            m_outputWindow, &TextOutputWindow::clearInfoMessage);

    m_outputWindow->sethookState(m_hookController->isRunning());
}

void MainWindow::initClipboardController()
{
    m_clipboardController = ClipboardController::create(this);

    if (!m_clipboardController) {
        Log(Logger::Level::Warning, "[Clipboard] No supported clipboard backend found"
                                    "Clipboard input mode is unavailable on this compositor");
        return;
    }

    connect(m_clipboardController, &ClipboardController::textChanged, this, [this](const QString &text) {
        setCurrentOutput(QStringLiteral("Clipboard"), text);
    });

    connect(m_clipboardController, &ClipboardController::failed, this, [this]() {
        m_clipboardController->stop();
        ui->textProcessingClipboardCheckBox->blockSignals(true);
        ui->textProcessingClipboardCheckBox->setChecked(false);
        ui->textProcessingClipboardCheckBox->blockSignals(false);
    });

    connect(m_outputWindow, &TextOutputWindow::internalClipboardWrite,
            m_clipboardController, &ClipboardController::suppress);

    if (ui->textProcessingClipboardCheckBox->isChecked())
        m_clipboardController->start();
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
    connect(ui->textProcessingHookCheckBox, &QCheckBox::stateChanged, this, bind(m_textProcessingChanged, ui->textProcessingHookCheckBox));
    connect(ui->textProcessingClipboardCheckBox, &QCheckBox::stateChanged, this, bind(m_textProcessingChanged, ui->textProcessingClipboardCheckBox));
    connect(ui->textProcessingTableWidget, &QTableWidget::currentItemChanged, this, bind(m_textProcessingChanged, ui->textProcessingTableWidget));
    connect(ui->textProcessingTableWidget, &QTableWidget::itemChanged, this, bind(m_textProcessingChanged, ui->textProcessingTableWidget));

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
    setupTextProcessingTable();

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

void MainWindow::setupTextProcessingTable()
{
    QTableWidget *table = ui->textProcessingTableWidget;

    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(ColRegex, QHeaderView::Interactive);
    header->setSectionResizeMode(ColSource, QHeaderView::Interactive);
    header->resizeSection(ColSource, 100);
    header->setSectionResizeMode(ColFrom, QHeaderView::Stretch);
    header->setSectionResizeMode(ColTo, QHeaderView::Stretch);

    table->setItemDelegateForColumn(ColRegex, new CheckBoxDelegate(table));
    table->setItemDelegateForColumn(ColSource, new SourceComboDelegate([] {
        return QStringList{
            QStringLiteral("Hook"),
            QStringLiteral("Clipboard"),
            QStringLiteral("Tesseract"),
            QStringLiteral("Ollama Vision")
        };
    }, table));
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
    m_pluginChanged = value;
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

#ifdef Q_OS_LINUX
void MainWindow::on_generalBindShortcut_clicked()
{
    m_hotkeyController->bindPortalShortcuts();
}
#endif

void MainWindow::on_outputGeneralSelect_clicked()
{
    m_captureController->openSourceSelector();
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
            m_translationController->setGoogleSourceLang(m_googleSourceLang);
            m_translationController->setGoogleTargetLang(m_googleTargetLang);

            m_translatorChanged = true;
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
        m_ocrController->isTesseractRunning() ? tr("Active") : tr("Inactive"),
        m_tesseractActiveLang.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(m_tesseractActiveLang)
    );

    m_tesseractSettingsDialog = new TesseractSettingsDialog(status,
                                                            m_tesseractSelectedLang,
                                                            m_tesserractLangList,
                                                            m_tesseractTessdataPath,
                                                            m_tesseractUseSystemTessdata,
                                                            m_tesseractMode,
                                                            m_tesseractAutoInterval,
                                                            m_ocrController->tesseract(),
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

void MainWindow::on_textProcessingHookSettingsButton_clicked()
{
    m_hookSettingsDialog = new HookSettingsDialog(m_hookMode, m_hookGameAppPluginList, m_hookEnginePluginList, m_currentGameAppPlugin, m_currentEnginePlugin, this);
    m_hookSettingsDialog->setWindowModality(Qt::WindowModal);
    m_hookSettingsDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(m_hookSettingsDialog, &QDialog::finished, this, [this](int result) {
        if (result == QDialog::Accepted) {
            m_hookMode = m_hookSettingsDialog->getCurrentMode();
            m_currentGameAppPlugin = m_hookSettingsDialog->getCurrentGameAppPlugin();
            m_currentEnginePlugin = m_hookSettingsDialog->getSelectedEngine();
            m_currentEngineProcess = m_hookSettingsDialog->getSelectedProcessName();

            m_textProcessingChanged = true;
            ui->textProcessingHookCheckBox->setProperty("changed", true);
            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
    });

    m_hookSettingsDialog->show();
}


QTableWidgetItem *MainWindow::makeRegexFlagItem(bool checked)
{
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

void MainWindow::on_textProcessingAddRowButton_clicked()
{
    int row = ui->textProcessingTableWidget->rowCount();
    ui->textProcessingTableWidget->insertRow(row);
    ui->textProcessingTableWidget->setItem(row, ColRegex, makeRegexFlagItem(false));
    ui->textProcessingTableWidget->setItem(row, ColSource, new QTableWidgetItem(QString()));
}

void MainWindow::on_textProcessingRemoveRowButton_clicked()
{
    int row = ui->textProcessingTableWidget->currentRow();

    if (row >= 0) {
        ui->textProcessingTableWidget->removeRow(row);
    }
}

void MainWindow::on_pluginsReloadButton_clicked()
{
    if (m_pluginManager) {
        if (m_hookController->isPluginLoaded()) {
            m_hookController->stop();
            m_hookController->setPlugin(nullptr);
        }
        m_pluginManager->unloadPlugins();
        m_hookGameAppPluginList.clear();
        ui->textProcessingHookCheckBox->setProperty("changed", true);

        initPlugins();
        loadHookPluginSettings();
    }
}

void MainWindow::on_pluginsOpenDirectoryButton_clicked()
{
    QDir dir(Config::getConfigDirPath() + "plugins/");
    if (dir.exists()) {
        QString path = dir.path();
        QUrl url = QUrl::fromLocalFile(path);
        QDesktopServices::openUrl(url);
    }
}

void MainWindow::on_logsNewLogMessage(const QString& message)
{
    ui->logsPlainText->appendPlainText(message);
}

void MainWindow::on_logsCopyAllButton_clicked()
{
    const QString text = ui->logsPlainText->toPlainText();
    if (m_clipboardController)
        m_clipboardController->suppress(text);

    QApplication::clipboard()->setText(text);
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

void MainWindow::setCurrentOriginalFrame(const QImage &frame)
{
    m_overlayImage = frame.copy();
    m_overlayImage.setDevicePixelRatio(this->devicePixelRatio());

    if (ui->listSettingsWidget->currentRow() == 1 && !frame.isNull()) {
        ui->outputOriginalScreencast->setPixmap(QPixmap::fromImage(m_overlayImage).scaled(ui->outputOriginalScreencast->size() * this->devicePixelRatio(), Qt::KeepAspectRatio));
    }

    m_captureController->notifyFrameProcessed();
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
    if (frame.empty()) return;
    m_ocrController->processFrame(frame);
}

void MainWindow::startScreenCapture()
{
    if (m_opencv) {
        m_opencv->setIsStopped(false);
    }
    m_captureController->start();
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

    m_captureController->stop();

    if (!m_overlayWindow->isHidden()) {
        m_overlayWindow->hide();
        m_outputWindow->show();
    }
}

void MainWindow::setCurrentOutput(const QString &source, const QString &output)
{
    QString outputFilt = replaceText(source, output);

    m_translationController->translate(source, outputFilt);

    // Hook
    if (source == "Hook") {
        m_outputWindow->clearInfoMessage();
        m_currentHookText = output;
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

            m_translationController->setOllamaUrl(m_ollamaUrl);
            m_translationController->setOllamaModel(m_ollamaCurrentModel);
            m_translationController->setOllamaPrompt(m_ollamaTranslationPrompt);

            m_ocrController->setOllamaUrl(m_ollamaUrl);
            m_ocrController->setOllamaModel(m_ollamaCurrentModel);
            m_ocrController->setOllamaVisionPrompt(m_ollamaVisionPrompt);
            m_ocrController->setOllamaVisionMode(m_ollamaVisionMode);
            m_ocrController->setOllamaVisionAutoInterval(m_ollamaVisionAutoInterval);
            m_ocrController->setOllamaWaitForResponse(m_waitForOllamaResponse);

            m_translatorChanged = true; m_textProcessingChanged = true;
            ui->textProcessingOCREngineOllamaVisionRadio->setProperty("changed", true);
            ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
    });

    m_ollamaSettingsDialog->show();
}

void MainWindow::retranslateText()
{
    if (!m_overlayWindow->getIsRectBrushEmpty() && ui->textProcessingOCREngineToggled->isChecked()) {
        m_ocrController->triggerManual();
    }

    if (ui->textProcessingHookCheckBox->isChecked() && !m_currentHookText.isEmpty()) {
        setCurrentOutput("Hook", m_currentHookText);
    }
}

void MainWindow::manualInjectHook()
{
    m_hookController->manualInject();
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

QString MainWindow::replaceText(const QString &source, QString output)
{
    for (int i = 0; i < ui->textProcessingTableWidget->rowCount(); ++i) {
        QTableWidgetItem* regexItem = ui->textProcessingTableWidget->item(i, ColRegex);
        QTableWidgetItem* srcItem = ui->textProcessingTableWidget->item(i, ColSource);
        QTableWidgetItem* fromItem = ui->textProcessingTableWidget->item(i, ColFrom);
        QTableWidgetItem* toItem = ui->textProcessingTableWidget->item(i, ColTo);

        if (!fromItem || fromItem->text().isEmpty())
            continue;

        const QString ruleSource = srcItem ? srcItem->text().trimmed() : QString();
        if (!ruleSource.isEmpty() && ruleSource.compare(source, Qt::CaseInsensitive) != 0)
            continue;

        const QString from = fromItem->text();
        const QString to = toItem ? toItem->text() : QString();
        const bool isRegex = regexItem && regexItem->checkState() == Qt::Checked;

        if (isRegex) {
            QRegularExpression re(from);
            if (re.isValid())
                output.replace(re, to);
        } else {
            output.replace(from, to);
        }
    }

    return output;
}

void MainWindow::initScreenCast()
{
    connect(m_captureController, &CaptureController::frameBufferReady,
            m_opencv, &OpenCV::setCurrentFrameBuffer);

    // Persist restore token from Portal session
    connect(m_captureController, &CaptureController::restoreTokenChanged, this,
            [](const QString &token) {
                QJsonObject screencast;
                screencast["restore_token"] = token;
                Config::setValue("screencast_portal", screencast);
                Config::saveConfig("settings.json");
            });

    // When the controller is about to tear down its backend, stop OpenCV first
    connect(m_captureController, &CaptureController::aboutToReconfigure, this, [this] {
        if (m_opencv) m_opencv->setIsStopped(true);
    });

    // After a new backend is producing frames, re-enable OpenCV
    connect(m_captureController, &CaptureController::captureRestarted, this, [this] {
        if (m_opencv) m_opencv->setIsStopped(false);
    });

    // Framerate
    m_captureController->setFramerate(ui->outputGeneralBoxFramerate->currentText());
    connect(ui->outputGeneralBoxFramerate, &QComboBox::currentTextChanged,
            m_captureController, &CaptureController::setFramerate);

    // OpenCV
    connect(m_opencv, &OpenCV::currentOriginalFrame, this, &MainWindow::setCurrentOriginalFrame);
    connect(m_opencv, &OpenCV::currentProcessedFrame, this, &MainWindow::setCurrentProcessedFrame);
    connect(m_opencv, &OpenCV::currentProcessedMat, this, &MainWindow::setCurrentProcessedMat);
    connect(m_overlayWindow, &OverlayWindow::currentRoi, m_opencv, &OpenCV::setCurrentRoi);
    connect(m_overlayWindow, &OverlayWindow::currentInnerRoi, m_opencv, &OpenCV::setCurrentIgnoreRoi);

    // Target config (loaded earlier into m_isCaptureDesktop / m_currentDisplay / m_currentWindow)
    m_captureController->setCaptureDesktop(m_isCaptureDesktop);
    if (m_isCaptureDesktop) {
        m_captureController->setDisplayIndex(m_currentDisplay);
    } else {
        m_captureController->setCurrentWindow(m_currentWindow);
    }

    m_captureController->initialize(m_currentRestoreToken);

    auto wireScreenCastWindow = [this] {
        if (auto *w = m_captureController->screenCastWindow()) {
            connect(w, &ScreenCastWindow::screencastWindowShown, this, [this, w] {
                connect(m_opencv, &OpenCV::currentOriginalFrame, w, &ScreenCastWindow::setCurrentOriginalFrame);
            });
            connect(w, &ScreenCastWindow::screencastWindowHidden, this, [this, w] {
                disconnect(m_opencv, &OpenCV::currentOriginalFrame, w, &ScreenCastWindow::setCurrentOriginalFrame);
            });
        }
    };
    wireScreenCastWindow();

#ifdef Q_OS_LINUX
    // If Portal fails at runtime, the controller swaps to ScreenCast backend
    // and creates a ScreenCastWindow. Re-attempt wiring after that happens
    connect(m_captureController, &CaptureController::captureFinished, this, wireScreenCastWindow);
#endif

    if (!ui->outputToggledScreencast->isChecked()) {
        m_captureController->start();
    }
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
    loadHookPluginSettings();

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
        m_translationController->setGoogleSourceLang(m_googleSourceLang);
        m_translationController->setGoogleTargetLang(m_googleTargetLang);
        m_translationController->setGoogleEnabled(ui->translatorOnlineGoogleToggled->isChecked());
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
        if (ollamaUrl != "") {
            m_ollamaUrl = ollamaUrl;
            m_ocrController->setOllamaUrl(m_ollamaUrl);
        }
        m_ollamaCurrentModel = ollama_translator["current_model"].toString();
        m_ollamaModels = ollama_translator["models"].toArray();
        m_ollamaTranslationPrompt = ollama_translator["translation_prompt"].toString();
        m_waitForOllamaResponse = ollama_translator["wait_for_responce"].toBool();

        m_translationController->setOllamaUrl(m_ollamaUrl);
        m_translationController->setOllamaModel(m_ollamaCurrentModel);
        m_translationController->setOllamaPrompt(m_ollamaTranslationPrompt);
        m_translationController->setOllamaEnabled(ui->translatorOfflineOllamaToggled->isChecked());
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

        m_ocrController->setTesseractMode(m_tesseractMode);
        m_ocrController->setTesseractAutoInterval(m_tesseractAutoInterval);

        // Ollama Vision
        QJsonObject ollama_vision = textProcessing["ollama_vision"].toObject();
        if (widgetChanged(ui->textProcessingOCREngineOllamaVisionRadio))
            ui->textProcessingOCREngineOllamaVisionRadio->setChecked(ollama_vision["is_vision"].toBool());

        m_ollamaVisionPrompt = ollama_vision["prompt"].toString();
        m_ollamaVisionMode = ollama_vision["mode"].toInt(Manual);
        m_ollamaVisionAutoInterval = ollama_vision["delay"].toInt(10);

        // HOOK
        QJsonObject hook = textProcessing["hook"].toObject();
        if (widgetChanged(ui->textProcessingHookCheckBox))
            ui->textProcessingHookCheckBox->setChecked(hook["is_hook"].toBool());

        m_hookMode = static_cast<HookSettingsDialog::HookMode>(hook["hook_mode"].toInt());
        m_currentGameAppPlugin = hook["current_game_app_plugin"].toString();
        m_currentEnginePlugin = hook["current_engine_plugin"].toString();
        m_currentEngineProcess = hook["current_engine_process"].toString();

        // Clipboard
        if (widgetChanged(ui->textProcessingClipboardCheckBox)) {
            bool isClipboard = textProcessing["is_clipboard"].toBool();
            ui->textProcessingClipboardCheckBox->blockSignals(true);
            ui->textProcessingClipboardCheckBox->setChecked(isClipboard);
            ui->textProcessingClipboardCheckBox->blockSignals(false);

            if (m_clipboardController) {
                if (isClipboard) {
                    m_clipboardController->start();
                } else {
                    m_clipboardController->stop();
                    m_outputWindow->clearResultsBySource(QStringLiteral("Clipboard"));
                }
            }
        }

        // Text Replacement
        QJsonArray jsonArray = textProcessing["text_replacement_table"].toArray();
        if (widgetChanged(ui->textProcessingTableWidget)) {
            ui->textProcessingTableWidget->setRowCount(jsonArray.size());
            for (int i = 0; i < jsonArray.size(); i++) {
                QJsonObject rowObject = jsonArray[i].toObject();
                ui->textProcessingTableWidget->setItem(i, ColFrom, new QTableWidgetItem(rowObject["from"].toString()));
                ui->textProcessingTableWidget->setItem(i, ColTo, new QTableWidgetItem(rowObject["to"].toString()));
                ui->textProcessingTableWidget->setItem(i, ColRegex, makeRegexFlagItem(rowObject["regex"].toBool()));
                ui->textProcessingTableWidget->setItem(i, ColSource, new QTableWidgetItem(rowObject["source"].toString()));
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

    if (m_captureController) {
        m_captureController->setCaptureDesktop(m_isCaptureDesktop);
        if (m_isCaptureDesktop) {
            m_captureController->setDisplayIndex(m_currentDisplay);
        } else {
            m_captureController->setCurrentWindow(m_currentWindow);
        }
    }
}

void MainWindow::loadOcrSettings()
{
    bool toggledChanged = widgetChanged(ui->textProcessingOCREngineToggled);
    bool isEnabled = ui->textProcessingOCREngineToggled->isChecked();

    if (!isEnabled && toggledChanged) {
        m_ocrController->setEnabled(false);
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

    m_ocrController->setEngine(m_ocrEngine == OcrEngine::Tesseract
                                   ? OcrController::Tesseract
                                   : OcrController::OllamaVision);
    m_ocrController->setEnabled(isEnabled);

    m_ocrController->setTesseractTessdataPath(m_tesseractTessdataPath);
    m_ocrController->setTesseractUseSystemTessdata(m_tesseractUseSystemTessdata);
    m_ocrController->setTesseractLanguage(m_tesseractActiveLang);
    m_ocrController->setTesseractMode(m_tesseractMode);
    m_ocrController->setTesseractAutoInterval(m_tesseractAutoInterval);

    m_ocrController->setOllamaVisionPrompt(m_ollamaVisionPrompt);
    m_ocrController->setOllamaVisionMode(m_ollamaVisionMode);
    m_ocrController->setOllamaVisionAutoInterval(m_ollamaVisionAutoInterval);
    m_ocrController->setOllamaWaitForResponse(m_waitForOllamaResponse);

    m_ocrController->applyConfiguration();

    // Refresh language list from Tesseract (only meaningful for Tesseract path).
    if (m_ocrEngine == OcrEngine::Tesseract && tesseractChanged) {
        m_tesserractLangList = m_ocrController->availableTesseractLanguages();
    }
}

void MainWindow::loadHookPluginSettings()
{
    if (!m_hookController->isPluginLoaded() || !widgetChanged(ui->textProcessingHookCheckBox)) {
        return;
    }

    m_hookController->setMode(m_hookMode);
    m_hookController->setCurrentGameAppPlugin(m_currentGameAppPlugin);
    m_hookController->setCurrentEnginePlugin(m_currentEnginePlugin);
    m_hookController->setCurrentEngineProcess(m_currentEngineProcess);

    m_hookController->apply(ui->textProcessingHookCheckBox->isChecked(),
                            ui->textProcessingHookCheckBox->isEnabled());
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
                emit restartRequested();
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

        QJsonObject translator_offline = translator.value("translator_offline").toObject();
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

        if (widgetChanged(ui->textProcessingOCREngineToggled))
            textProcessing["is_ocr"] = ui->textProcessingOCREngineToggled->isChecked();

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

        // Hook
        QJsonObject hook = textProcessing.value("hook").toObject();
        if (widgetChanged(ui->textProcessingHookCheckBox))
            hook["is_hook"] = ui->textProcessingHookCheckBox->isChecked();

        hook["hook_mode"] = m_hookMode;
        hook["current_game_app_plugin"] = m_currentGameAppPlugin;
        hook["current_engine_plugin"] = m_currentEnginePlugin;
        hook["current_engine_process"] = m_currentEngineProcess;

        textProcessing.insert("tesseract", tesseract);
        textProcessing.insert("ollama_vision", ollama_vision);
        textProcessing.insert("hook", hook);

        if (widgetChanged(ui->textProcessingClipboardCheckBox))
            textProcessing["is_clipboard"] = ui->textProcessingClipboardCheckBox->isChecked();

        if (widgetChanged(ui->textProcessingTableWidget)) {
            QJsonArray jsonArray;
            for (int i = 0; i < ui->textProcessingTableWidget->rowCount(); i++) {
                QTableWidgetItem *regexItem = ui->textProcessingTableWidget->item(i, ColRegex);
                QTableWidgetItem *srcItem = ui->textProcessingTableWidget->item(i, ColSource);
                QTableWidgetItem *fromItem = ui->textProcessingTableWidget->item(i, ColFrom);
                QTableWidgetItem *toItem = ui->textProcessingTableWidget->item(i, ColTo);

                QJsonObject rowObject;
                rowObject["regex"] = regexItem && regexItem->checkState() == Qt::Checked;
                rowObject["source"] = srcItem ? srcItem->text().trimmed() : QString();
                rowObject["from"] = fromItem ? fromItem->text() : QString();
                rowObject["to"] = toItem ? toItem->text() : QString();
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