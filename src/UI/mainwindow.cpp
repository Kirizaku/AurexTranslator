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

#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QTimer>

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
#include "src/controllers/pythoncontroller.h"
#include "src/engines/audioplayer.h"
#include "src/utils/notificationsound.h"
#include "src/engines/pipertts.h"
#include "src/engines/edgetts.h"
#include "src/engines/customtts.h"
#include "src/utils/plugininterface.h"
#include "src/utils/logger.h"
#include "src/utils/config.h"
#include "src/utils/dialogutils.h"
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
    initPythonController();
    initSpeech();
    setupCoreConnections();
    loadApplicationConfig();
    initSubsystems();
    initClipboardController();
    setupSettingsConnections();

    connect(m_translationController, &TranslationController::translationReady,
            m_outputWindow, &TextOutputWindow::setTranslationResult);
    connect(m_translationController, &TranslationController::originalReady,
            m_outputWindow, &TextOutputWindow::setOriginalText);

    m_hookBurstTimer = new QTimer(this);
    m_hookBurstTimer->setSingleShot(true);
    connect(m_hookBurstTimer, &QTimer::timeout, this, [this] { flushHookBurst(); });

    loadLogMessages();
    setupFinalUI();
    activateProfileForHook();
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

    if (m_audioPlayer) { m_audioPlayer->stop(); }
    for (TtsEngine *engine : m_ttsEngines) { engine->stop(); }

    if (m_overlayWindow) { delete m_overlayWindow; }
    if (m_outputWindow) { delete m_outputWindow; }

    Logger::instance()->destroyInstance();
    Config::instance()->destroyInstance();

    delete ui;
}

// ===============================================================
// ui slots: general
// ===============================================================

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
    } else {
        discardPendingProfileOverride();
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

void MainWindow::on_textProcessingAddRowButton_clicked()
{
    int row = ui->textProcessingTableWidget->rowCount();
    ui->textProcessingTableWidget->insertRow(row);
    ui->textProcessingTableWidget->setItem(row, ColRegex, makeRegexFlagItem(false));
    ui->textProcessingTableWidget->setItem(row, ColSource, new QTableWidgetItem(QString()));
    commitReplacementTable();
    markRulesChanged();
}

void MainWindow::on_textProcessingRemoveRowButton_clicked()
{
    int row = ui->textProcessingTableWidget->currentRow();

    if (row >= 0) {
        ui->textProcessingTableWidget->removeRow(row);
        commitReplacementTable();
        markRulesChanged();
    }
}

// ===============================================================
// configs
// ===============================================================

void MainWindow::on_configsNewButton_clicked()
{
    bool ok = false;
    const QString name = DialogUtils::getText(this,
                                              tr("New profile"),
                                              tr("Profile name (snapshots the current settings):"),
                                              QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    if (Config::availableProfiles().contains(name)) {
        DialogUtils::warning(this,
                             tr("New profile"),
                             tr("A profile named '%1' already exists.").arg(name));
        return;
    }

    Config::saveCurrentAsProfile(name); // writes configs/<name>.json + switches active
    refreshConfigsPage();
}

void MainWindow::on_configsLoadButton_clicked()
{
    QListWidgetItem *item = ui->configsListWidget->currentItem();
    if (!item) return;

    const QString name = item->data(Qt::UserRole).toString();
    if (name == Config::activeProfile()) return;

    if (!Config::loadProfile(name)) {
        DialogUtils::warning(this,
                             tr("Load profile"),
                             tr("Could not load profile '%1'.").arg(name));
        return;
    }
    reapplyProfileSections();
    refreshConfigsPage();
}

void MainWindow::on_configsRenameButton_clicked()
{
    QListWidgetItem *item = ui->configsListWidget->currentItem();
    if (!item) return;

    const QString oldName = item->data(Qt::UserRole).toString();

    bool ok = false;

    const QString newName = DialogUtils::getText(this,
                                                 tr("Rename profile"),
                                                 tr("New name:"), QLineEdit::Normal, oldName, &ok).trimmed();

    if (!ok || newName.isEmpty() || newName == oldName)
        return;

    if (Config::availableProfiles().contains(newName)) {
        DialogUtils::warning(this,
                             tr("Rename profile"),
                             tr("A profile named '%1' already exists.").arg(newName));
        return;
    }

    if (!Config::renameProfile(oldName, newName))
        DialogUtils::warning(this, tr("Rename profile"), tr("Rename failed."));

    refreshConfigsPage();
}

void MainWindow::on_configsDeleteButton_clicked()
{
    QListWidgetItem *item = ui->configsListWidget->currentItem();
    if (!item) return;

    const QString name = item->data(Qt::UserRole).toString();
    if (name == Config::activeProfile()) {
        DialogUtils::information(this,
                                 tr("Delete profile"),
                                 tr("The active profile can't be deleted. Load another profile first."));
        return;
    }

    if (DialogUtils::question(this,
                              tr("Delete profile"),
                              tr("Delete profile '%1'? This cannot be undone.").arg(name)) != QMessageBox::Yes) {
        return;
    }

    Config::deleteProfile(name);
    refreshConfigsPage();
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

// ===============================================================
// speech
// ===============================================================

void MainWindow::on_speechEngineCombo_currentIndexChanged(int arg1)
{
    Q_UNUSED(arg1)

    TtsEngine *chosen = ttsEngine(ui->speechEngineCombo->currentData().toString());
    if (!chosen || chosen == m_tts)
        return;

    setTtsEngine(chosen);
    markSpeechChanged();
}

void MainWindow::on_speechEngineSettingsButton_clicked()
{
    QDialog *dialog = m_tts->createSettingsDialog(this);

    if (!dialog)
        return;

    dialog->setWindowModality(Qt::WindowModal);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    TtsEngine *engine = m_tts;
    const QJsonObject before = engine->saveSettings();

    connect(dialog, &QDialog::finished, this, [this, engine, before](int) {
        const QJsonObject after = engine->saveSettings();
        const bool changed = after != before;

        refreshSpeechVoices();

        if (changed) {
            markSpeechChanged();

            if (m_speaking == engine && engine->settingsChangeNeedsRestart(before, after))
                startSpeech();
        }

        if (!m_speechOn && engine != m_speaking)
            engine->stop();

        refreshSpeechPage();
    });

    dialog->show();
}

void MainWindow::on_speechTestButton_clicked()
{
    const QString text = ui->speechTestEdit->text().trimmed();

    if (text.isEmpty())
        return;

    m_speechPending.clear();
    m_audioPlayer->stop();
    m_tts->synthesize(replaceText(QStringLiteral("Speech"), text));

    refreshSpeechPage();
}

void MainWindow::on_speechStopButton_clicked()
{
    m_speechPending.clear();

    if (m_speaking)
        m_speaking->cancelSynthesis();

    if (m_tts && m_tts != m_speaking)
        m_tts->cancelSynthesis();

    m_audioPlayer->stop();
    refreshSpeechPage();
}

// ===============================================================
// python
// ===============================================================

void MainWindow::on_pythonRecheckButton_clicked()
{
    m_pythonController->setPreferredInterpreter(ui->pythonInterpreterEdit->text());
    m_pythonController->detect();
}

void MainWindow::on_pythonSetupButton_clicked()
{
    m_pythonController->setupEnvironment();
    updatePythonButtons();
}

#ifdef Q_OS_WIN
void MainWindow::on_pythonInstallPythonButton_clicked()
{
    QMessageBox box(QMessageBox::Question,
                    tr("Install Python"),
                    tr("Python %1 will be downloaded from python.org (about 30 MB).").arg(PythonEnv::windowsPythonVersion()),
                    QMessageBox::NoButton,
                    this);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    box.setInformativeText(
        tr("For this program only — installed into its own folder, adds nothing to PATH "
           "and leaves the rest of the system alone.\n\n"
           "System-wide — an ordinary installation, available to other programs as well."));

    QPushButton *privateButton = box.addButton(tr("For this program only"), QMessageBox::AcceptRole);
    QPushButton *systemButton = box.addButton(tr("System-wide"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(privateButton);
    box.exec();

    QAbstractButton *chosen = box.clickedButton();
    if (chosen != privateButton && chosen != systemButton)
        return;

    m_pythonController->installPython(chosen == systemButton);
    updatePythonButtons();
}
#endif

void MainWindow::on_pythonInterpreterBrowseButton_clicked()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select a Python interpreter"),
                                                      ui->pythonInterpreterEdit->text());
    if (path.isEmpty())
        return;

    ui->pythonInterpreterEdit->setText(path);
}

void MainWindow::on_pythonOpenDirectoryButton_clicked()
{
    QDir dir(PythonEnv::rootDir());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QDesktopServices::openUrl(QUrl::fromLocalFile(dir.path()));
}

void MainWindow::on_pythonShowLogButton_clicked()
{
    m_pythonController->showLog();
}

void MainWindow::on_pythonComponentInstallButton_clicked()
{
    const QTableWidgetItem *row = selectedPythonRow();

    if (!row)
        return;

    m_pythonController->installComponent(row->data(Qt::UserRole).toString(), row->data(Qt::UserRole + 1).toBool());
    updatePythonButtons();
}

void MainWindow::on_pythonComponentRemoveButton_clicked()
{
    const QString id = selectedPythonComponent();
    if (id.isEmpty())
        return;

    const PythonController::Component *component = m_pythonController->component(id);
    if (!component)
        return;

    const QStringList packages = m_pythonController->removablePackages(id);

    if (packages.isEmpty()) {
        DialogUtils::information(this, tr("Remove component"),
                                 tr("Everything %1 installed is also being used by another "
                                    "component, so there is nothing here to remove.")
                                     .arg(component->name));
        return;
    }

    if (DialogUtils::question(
            this, tr("Remove component"),
            tr("Remove %1?\n\nThese packages go: %2\n\nAnything they pulled in with them "
               "stays, and so does anything downloaded separately - voices and language "
               "packs live in the engine's own folder and are left alone.").arg(component->name, packages.join(QStringLiteral(", "))))
        != QMessageBox::Yes)
        return;

    m_pythonController->uninstallComponent(id);
    updatePythonButtons();
}

// ===============================================================
// logs and proxy
// ===============================================================

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

// ===============================================================
// opencv
// ===============================================================

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

// ===============================================================
// screen capture
// ===============================================================

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

// ===============================================================
// utility actions
// ===============================================================

void MainWindow::setCurrentOutput(const QString &source, const QString &output)
{
    QString outputFilt = replaceText(source, output);

    if (source.startsWith(QStringLiteral("Hook"))) {
        m_currentHookTexts.insert(source, output);
        m_outputWindow->noteHookSource(source, output);

        const QString effSource = m_outputWindow->hookEffectiveSource(source);
        const QString effText = (effSource == source)
            ? outputFilt
            : replaceText(effSource, m_outputWindow->hookCombinedOriginal(effSource));

        m_hookBurstBuffer.insert(effSource, effText);

        if (!m_hookBurstTimer->isActive())
            m_hookBurstTimer->start(m_outputWindow->hookBurstMs());
        return;
    }

    m_translationController->translate(source, outputFilt);
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

    if (ui->textProcessingHookCheckBox->isChecked()) {
        for (auto it = m_currentHookTexts.cbegin(); it != m_currentHookTexts.cend(); ++it) {
            if (!it.value().isEmpty())
                setCurrentOutput(it.key(), it.value());
        }
    }
}

void MainWindow::manualInjectHook()
{
    m_hookController->manualInject();
}

void MainWindow::reapplyHookOutput()
{
    QHash<QString, QString> effTexts;
    const QStringList displayed = m_outputWindow->hookDisplayOrder();
    for (const QString &src : displayed) {
        const QString orig = m_outputWindow->hookLatestOriginal(src);
        if (!orig.isEmpty())
            effTexts.insert(src, orig);
    }
    if (effTexts.isEmpty())
        return;

    const QStringList winners = m_outputWindow->hookSourcesToOutput(effTexts.keys());
    QHash<QString, QString> toSend;
    QStringList sendOrder;
    for (const QString &src : winners) {
        auto it = effTexts.constFind(src);
        if (it == effTexts.constEnd() || it.value().isEmpty())
            continue;
        const QString filtered = replaceText(src, it.value());
        if (m_outputWindow->hasHookTranslation(src, filtered))
            continue;
        toSend.insert(src, filtered);
        sendOrder << src;
    }

    m_outputWindow->beginHookBatch(sendOrder);
    for (const QString &src : sendOrder)
        m_translationController->translate(src, toSend.value(src));
}

// ===============================================================
// output window
// ===============================================================

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

// ===============================================================
// startup
// ===============================================================

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
        ui->generalHotkeySpeakTextEdit,
        ui->generalHotkeyStopSpeechEdit,
        ui->generalHotkeyToggleSpeechEdit,
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
        ui->proxyTypeSocks,
        ui->pythonInterpreterEdit,
        ui->speechEnabled,
        ui->speechToggleSoundCheckBox,
        ui->speechOfferPresetsCheckBox,
        ui->speechTextCombo,
        ui->speechSourceCombo,
        ui->speechTranslatorCombo,
        ui->speechModeCombo,
        ui->speechOnNewTranslationCombo,
        ui->speechVolumeSlider
    };
}

void MainWindow::initPlugins()
{
    m_registry = m_pluginManager->scanPlugins();
    m_pluginManager->loadPlugins();

    ui->textProcessingHookRow->setVisible(false);
    ui->textProcessingHookCheckBox->setEnabled(false);

    m_hookGameAppPluginList.clear();
    m_hookEnginePluginList.clear();

    QMap<QString, QStringList> dependencyErrors = m_pluginManager->validateDependencies(m_registry);

    bool hookPluginReady = false;
    const auto injectorIt = std::find_if(m_registry.begin(), m_registry.end(),
                                         [](const PluginManager::PluginInfo &i) { return i.name == "libat-injector"; });

    if (injectorIt != m_registry.end() && !dependencyErrors.contains("libat-injector")) {
        if (m_hookController->isPluginLoaded()) {
            hookPluginReady = true;
        } else {
            QObject* pluginObj = m_pluginManager->getPlugin("libat-injector");
            PluginInterface *hookPlugin = qobject_cast<PluginInterface*>(pluginObj);

            if (hookPlugin) {
                m_hookController->setPlugin(hookPlugin);
                hookPluginReady = true;
            } else {
                Log(Logger::Level::Warning, "[Hook] Failed to load plugin 'libat-injector'");
                m_registry.erase(injectorIt);
                dependencyErrors = m_pluginManager->validateDependencies(m_registry);
            }
        }
    }

    m_hookController->setRegistry(m_registry);

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

        if (hasErrors) {
            Log(Logger::Level::Warning, QString("[plugin-loader] Plugin '%1' is invalid. Missing dependencies: %2")
                                            .arg(p.name, missingList.join(", ")));
        }
    }

    ui->textProcessingHookRow->setVisible(hookPluginReady);
    ui->textProcessingHookCheckBox->setEnabled(hookPluginReady && dependencyErrors.isEmpty());

    // Auto select for games
    ui->textProcessingBindingRow->setVisible(hookPluginReady);
    ui->textProcessingBindingHelp->setVisible(hookPluginReady);

    // Push the current target's plugin config again: a reload dropped whatever
    // the plugin had been told before
    syncPluginConfigs();
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
    connect(m_outputWindow, &TextOutputWindow::hookOutputReapplyRequested, this, &MainWindow::reapplyHookOutput);
    connect(m_outputWindow, &TextOutputWindow::toggleSpeechRequested, this, [this] { toggleSpeech(false); });
    connect(m_outputWindow, &TextOutputWindow::speakLastRequested, this, &MainWindow::speakLastText);
    connect(m_outputWindow, &TextOutputWindow::stopSpeechRequested, this, &MainWindow::on_speechStopButton_clicked);
    m_outputWindow->setSpeechEnabled(m_speechOn);
    m_outputWindow->setSpeechBusy(speechBusy());

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
        m_hotkeyController->setToggleSpeechShortcut(ui->generalHotkeyToggleSpeechEdit->keySequence());
        m_hotkeyController->setSpeakTextShortcut(ui->generalHotkeySpeakTextEdit->keySequence());
        m_hotkeyController->setStopSpeechShortcut(ui->generalHotkeyStopSpeechEdit->keySequence());

        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setCaptureRegionShortcut);
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setShowHistoryShortcut);
        connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setRetranslateShortcut);
        connect(ui->generalHotkeyToggleSpeechEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setToggleSpeechShortcut);
        connect(ui->generalHotkeySpeakTextEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setSpeakTextShortcut);
        connect(ui->generalHotkeyStopSpeechEdit, &QKeySequenceEdit::keySequenceChanged,
                m_hotkeyController, &HotkeyController::setStopSpeechShortcut);

        connect(ui->generalHotkeySelectNewRegionEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeySelectNewRegionEdit->clearFocus(); });
        connect(ui->generalHotkeyHistoryTranslationEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeyHistoryTranslationEdit->clearFocus(); });
        connect(ui->generalHotkeyManualTranslateEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeyManualTranslateEdit->clearFocus(); });
        connect(ui->generalHotkeyToggleSpeechEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeyToggleSpeechEdit->clearFocus(); });
        connect(ui->generalHotkeySpeakTextEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeySpeakTextEdit->clearFocus(); });
        connect(ui->generalHotkeyStopSpeechEdit, &QKeySequenceEdit::editingFinished, this,
                [this] { ui->generalHotkeyStopSpeechEdit->clearFocus(); });
    }

    connect(m_hotkeyController, &HotkeyController::captureRegionTriggered,
            this, &MainWindow::captureRegion);
    connect(m_hotkeyController, &HotkeyController::showHistoryTriggered,
            this, &MainWindow::showHistory);
    connect(m_hotkeyController, &HotkeyController::retranslateTriggered,
            this, &MainWindow::retranslateText);
    connect(m_hotkeyController, &HotkeyController::toggleSpeechTriggered,
            this, [this] { toggleSpeech(true); });
    connect(m_hotkeyController, &HotkeyController::speakTextTriggered,
            this, &MainWindow::speakLastText);
    connect(m_hotkeyController, &HotkeyController::stopSpeechTriggered,
            this, &MainWindow::on_speechStopButton_clicked);
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
    connect(m_hookController, &HookController::hookStateChanged,
            this, &MainWindow::updateSpeedButtonAvailability);
    connect(m_hookController, &HookController::hookStateChanged,
            this, [this] { activateProfileForHook(); });
    connect(m_hookController, &HookController::hookStateChanged, this, [this](bool active) {
        if (active)
            pushCurrentPluginConfig();
    });
    connect(m_outputWindow, &TextOutputWindow::speedSettingsRequested,
            this, &MainWindow::openSpeedSettings);
    connect(m_hookController, &HookController::shouldClearResults,
            this, &MainWindow::clearHookState);
    connect(m_outputWindow, &TextOutputWindow::hookClearRequested,
            this, &MainWindow::clearHookState);
    connect(m_hookController, &HookController::shouldClearInfoMessage,
            m_outputWindow, &TextOutputWindow::clearInfoMessage);

    connect(ui->configsListWidget, &QListWidget::itemSelectionChanged, this, [this] {
        QListWidgetItem *sel = ui->configsListWidget->currentItem();
        const QString name = sel ? sel->data(Qt::UserRole).toString() : QString();
        const bool hasSel = sel != nullptr;
        const bool isActive = hasSel && name == Config::activeProfile();
        ui->configsLoadButton->setEnabled(hasSel && !isActive);
        ui->configsRenameButton->setEnabled(hasSel);
        ui->configsDeleteButton->setEnabled(hasSel && !isActive);
    });
    refreshConfigsPage();

    m_outputWindow->sethookState(m_hookController->isRunning());
    updateSpeedButtonAvailability();
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
    connect(ui->generalHotkeyToggleSpeechEdit, &QKeySequenceEdit::keySequenceChanged, this, bind(m_generalChanged, ui->generalHotkeyToggleSpeechEdit));
    connect(ui->generalHotkeySpeakTextEdit, &QKeySequenceEdit::keySequenceChanged, this, bind(m_generalChanged, ui->generalHotkeySpeakTextEdit));
    connect(ui->generalHotkeyStopSpeechEdit, &QKeySequenceEdit::keySequenceChanged, this, bind(m_generalChanged, ui->generalHotkeyStopSpeechEdit));
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
    connect(ui->textProcessingTableWidget, &QTableWidget::itemChanged, this, [this] { commitReplacementTable(); });

    connect(ui->textProcessingBindingBox, &QComboBox::currentIndexChanged, this, [this](int) {
        ui->textProcessingBindingRemoveButton->setEnabled(!ui->textProcessingBindingBox->currentData().toString().isEmpty());
    });

    connect(ui->textProcessingBindingAddButton, &QPushButton::clicked, this, &MainWindow::addProfileBinding);
    connect(ui->textProcessingBindingRemoveButton, &QPushButton::clicked, this, &MainWindow::removeProfileBinding);

    connect(ui->textProcessingProfileBox, &QComboBox::currentIndexChanged, this, [this](int) {
        commitReplacementTable();

        m_activeProfile = ui->textProcessingProfileBox->currentData().toString();
        m_profileHookState = ProfileHookState::ManualOverride;

        const bool custom = !m_activeProfile.isEmpty();

        ui->textProcessingProfileRenameButton->setEnabled(custom);
        ui->textProcessingProfileDeleteButton->setEnabled(custom);

        markRulesChanged();
        refreshBindingBox();
        populateReplacementTable();
    });
    connect(ui->textProcessingProfileNewButton, &QPushButton::clicked, this, &MainWindow::addRuleProfile);
    connect(ui->textProcessingProfileRenameButton, &QPushButton::clicked, this, &MainWindow::renameRuleProfile);
    connect(ui->textProcessingProfileDeleteButton, &QPushButton::clicked, this, &MainWindow::deleteRuleProfile);

    // Proxy
    connect(ui->proxyEnabledCheckBox, &QCheckBox::stateChanged, this, bind(m_proxyChanged, ui->proxyEnabledCheckBox));
    connect(ui->proxyAddressEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyAddressEdit));
    connect(ui->proxyPortEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyPortEdit));
    connect(ui->proxyUserEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyUserEdit));
    connect(ui->proxyPasswordEdit, &QLineEdit::textChanged, this, bind(m_proxyChanged, ui->proxyPasswordEdit));
    connect(ui->proxyTypeHttp, &QRadioButton::toggled, this, bind(m_proxyChanged, ui->proxyTypeHttp));
    connect(ui->proxyTypeSocks, &QRadioButton::toggled, this, bind(m_proxyChanged, ui->proxyTypeSocks));

    // Python
    connect(ui->pythonInterpreterEdit, &QLineEdit::textChanged, this, bind(m_pythonChanged, ui->pythonInterpreterEdit));

    // Speech
    connect(ui->speechEnabled, &QCheckBox::stateChanged, this, bind(m_speechChanged, ui->speechEnabled));
    connect(ui->speechToggleSoundCheckBox, &QCheckBox::stateChanged, this, bind(m_speechChanged, ui->speechToggleSoundCheckBox));
    connect(ui->speechOfferPresetsCheckBox, &QCheckBox::stateChanged, this, bind(m_speechChanged, ui->speechOfferPresetsCheckBox));
    connect(ui->speechTextCombo, &QComboBox::currentIndexChanged, this, bind(m_speechChanged, ui->speechTextCombo));
    connect(ui->speechSourceCombo, &QComboBox::currentIndexChanged, this, bind(m_speechChanged, ui->speechSourceCombo));
    connect(ui->speechTranslatorCombo, &QComboBox::currentIndexChanged, this, bind(m_speechChanged, ui->speechTranslatorCombo));
    connect(ui->speechOnNewTranslationCombo, &QComboBox::currentIndexChanged, this, bind(m_speechChanged, ui->speechOnNewTranslationCombo));
    connect(ui->speechModeCombo, &QComboBox::currentIndexChanged, this, bind(m_speechChanged, ui->speechModeCombo));
    connect(ui->speechVolumeSlider, &QSlider::valueChanged, this, bind(m_speechChanged, ui->speechVolumeSlider));
    connect(ui->speechModeCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { refreshSpeechPage(); });
    connect(ui->speechTextCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_speechSpokenOriginals.clear();
        refreshSpeechPage();
    });
    connect(ui->speechTranslatorCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { m_speechSpokenOriginals.clear(); });
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
    table->setItemDelegateForColumn(ColSource, new SourceComboDelegate([this] {
        QStringList sources{
            QStringLiteral("Hook"),
            QStringLiteral("Clipboard"),
            QStringLiteral("Tesseract"),
            QStringLiteral("Ollama Vision"),
            QStringLiteral("Speech")
        };
        const QStringList hookSources = m_outputWindow->hookDisplayOrder();
        for (const QString &src : hookSources) {
            if (!sources.contains(src))
                sources << src;
        }
        return sources;
    }, table));

    setupReplacementPresets();
    refreshProfileBox();
}

// ===============================================================
// python
// ===============================================================

void MainWindow::initPythonController()
{
    m_pythonController = new PythonController(this);

    connect(m_pythonController, &PythonController::statusChanged,
            this, &MainWindow::refreshPythonPage);
    connect(m_pythonController, &PythonController::jobFinished, this,
            [this](const QString &, bool ok, const QString &error) {
                if (!ok && !error.isEmpty())
                    DialogUtils::warning(this, "Python", error);
            });

    connect(ui->pythonComponentsTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::updatePythonButtons);

    m_pythonController->setConfirmHandler([this](const PythonController::Component &component) {
        return confirmPythonDownload(component);
    });

#ifndef Q_OS_WIN
    ui->pythonInstallPythonButton->hide();
#endif

    m_pythonController->setPreferredInterpreter(
        Config::getValue("python").toJsonObject()["interpreter"].toString());

    connect(ui->settingsPages, &QStackedWidget::currentChanged, this, [this](int) {
        if (ui->settingsPages->currentWidget() == ui->pythonPage)
            m_pythonController->ensureDetected();
    });

    refreshPythonPage();
}

void MainWindow::refreshPythonPage()
{
    const QString interpreter = m_pythonController->interpreterDescription();

    ui->pythonInterpreterLabel->setText(interpreter.isEmpty() ? tr("Not found") : interpreter);
    ui->pythonEnvironmentLabel->setText(PythonController::environmentReady()
                                            ? tr("Ready")
                                            : tr("Not created yet"));

    ui->pythonInterpreterLabel->setToolTip(interpreter);
    ui->pythonEnvironmentLabel->setToolTip(QDir::toNativeSeparators(PythonEnv::venvDir()));

    const QList<PythonController::Component> components = m_pythonController->components();
    const QSignalBlocker blocker(ui->pythonComponentsTable);
    ui->pythonComponentsTable->setRowCount(components.size());

    for (int row = 0; row < components.size(); ++row) {
        const PythonController::Component &component = components.at(row);
        const bool ready = m_pythonController->componentReady(component.id);

        QTableWidgetItem *name = new QTableWidgetItem(component.name);
        name->setData(Qt::UserRole, component.id);
        name->setData(Qt::UserRole + 1, ready);

        ui->pythonComponentsTable->setItem(row, 0, name);
        ui->pythonComponentsTable->setItem(row, 1,
            new QTableWidgetItem(ready ? tr("Installed") : tr("Not installed")));
        ui->pythonComponentsTable->setItem(row, 2,
            new QTableWidgetItem(component.packages.join(", ")));
    }

    updatePythonButtons();
}

void MainWindow::updatePythonButtons()
{
    const bool interpreterFound = m_pythonController->interpreterReady();
    const bool busy = m_pythonController->busy();

    ui->pythonSetupButton->setText(PythonController::environmentReady()
                                       ? tr("Update environment")
                                       : tr("Create environment"));

    ui->pythonSetupButton->setEnabled(!busy && interpreterFound);
    ui->pythonRecheckButton->setEnabled(!busy);
    ui->pythonInstallPythonButton->setEnabled(!busy);

    const QTableWidgetItem *row = selectedPythonRow();
    const bool installed = row && row->data(Qt::UserRole + 1).toBool();

    ui->pythonComponentInstallButton->setText(installed ? tr("Reinstall") : tr("Install"));
    ui->pythonComponentInstallButton->setEnabled(!busy && interpreterFound && row);
    ui->pythonComponentRemoveButton->setEnabled(!busy && installed);
}

QTableWidgetItem *MainWindow::selectedPythonRow() const
{
    return ui->pythonComponentsTable->item(ui->pythonComponentsTable->currentRow(), 0);
}

QString MainWindow::selectedPythonComponent() const
{
    const QTableWidgetItem *row = selectedPythonRow();
    return row ? row->data(Qt::UserRole).toString() : QString();
}

bool MainWindow::confirmPythonDownload(const PythonController::Component &component)
{
    QMessageBox box(QMessageBox::Question,
                    tr("Install %1").arg(component.name),
                    tr("These packages will be installed from PyPI:\n\n%1").arg(component.packages.join(QStringLiteral("\n"))),
                    QMessageBox::Ok | QMessageBox::Cancel,
                    this);

    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);

    QString informative = tr("They are third-party code under their own licenses.\n"
                             "Destination: %1").arg(QDir::toNativeSeparators(PythonEnv::venvDir()));

    if (!component.note.isEmpty())
        informative.prepend(component.note + QStringLiteral("\n\n"));

    box.setInformativeText(informative);
    box.button(QMessageBox::Ok)->setText(tr("Install"));
    box.setDefaultButton(QMessageBox::Ok);

    return box.exec() == QMessageBox::Ok;
}

// ===============================================================
// speech
// ===============================================================

TtsEngine *MainWindow::ttsEngine(const QString &id) const
{
    for (TtsEngine *engine : m_ttsEngines) {
        if (engine->id() == id)
            return engine;
    }

    return nullptr;
}

void MainWindow::setTtsEngine(TtsEngine *engine)
{
    if (m_tts == engine)
        return;

    m_tts = engine;

    {
        const QSignalBlocker blocker(ui->speechVoiceCombo);
        ui->speechVoiceCombo->clear();
    }

    {
        const QSignalBlocker blocker(ui->speechEngineCombo);
        const int index = ui->speechEngineCombo->findData(engine->id());
        if (index >= 0)
            ui->speechEngineCombo->setCurrentIndex(index);
    }

    ui->speechTestEdit->setText(m_tts->sampleText());

    refreshSpeechSpeed();
    refreshSpeechVoices();
    refreshSpeechPage();

    QTimer::singleShot(0, this, &MainWindow::maybeSuggestLanguagePreset);
}

void MainWindow::registerTtsEngine(TtsEngine *engine, const PythonController::Component &component)
{
    m_ttsEngines.append(engine);

    if (!component.id.isEmpty())
        m_pythonController->registerComponent(component);

    const QString prefix = QStringLiteral("[%1] ").arg(engine->id());

    connect(engine, &TtsEngine::logLine, this, [prefix](const QString &line) {
        Log(Logger::Level::Info, prefix + line);
    });

    connect(engine, &TtsEngine::errorOccurred, this, [this, engine](const QString &error) {
        if (engine == m_speaking)
            m_speechPending.clear();

        if (engine == m_tts || engine == m_speaking)
            refreshSpeechPage();

        if (engine == m_tts)
            ui->speechStatusLabel->setText(error);
    });

    connect(engine, &TtsEngine::audioReady, this, [this, engine](const QByteArray &wav) {
        if (engine == m_speaking || engine == m_tts)
            m_audioPlayer->play(wav);
    });

    connect(engine, &TtsEngine::stateChanged, this, [this, engine](TtsEngine::State) {
        if (engine == m_tts || engine == m_speaking)
            refreshSpeechPage();
    });

    connect(engine, &TtsEngine::voicesAvailable, this, [this, engine](const QStringList &) {
        if (engine != m_tts)
            return;

        const QString before = ui->speechVoiceCombo->currentData().toString();
        refreshSpeechVoices();

        if (ui->speechVoiceCombo->currentData().toString() != before)
            ui->speechTestEdit->setText(m_tts->sampleText());
    });
}

void MainWindow::populateSpeechEngines()
{
    const QSignalBlocker blocker(ui->speechEngineCombo);
    ui->speechEngineCombo->clear();

    const QList<QPair<TtsEngine::Kind, QString>> groups{
        {TtsEngine::Kind::Offline, tr("On this machine")},
        {TtsEngine::Kind::Online, tr("Over the internet")},
        {TtsEngine::Kind::Custom, tr("Your own server")}};

        for (const QPair<TtsEngine::Kind, QString> &group : groups) {
            bool headed = false;

            for (TtsEngine *engine : std::as_const(m_ttsEngines)) {
                if (engine->kind() != group.first)
                    continue;

                if (!headed) {
                    addSpeechEngineHeading(group.second);
                    headed = true;
                }

                ui->speechEngineCombo->addItem(engine->name(), engine->id());
            }
        }
}

void MainWindow::addSpeechEngineHeading(const QString &text)
{
    ui->speechEngineCombo->addItem(text);

    QStandardItemModel *model =
    qobject_cast<QStandardItemModel *>(ui->speechEngineCombo->model());
    if (!model)
        return;

    QStandardItem *item = model->item(ui->speechEngineCombo->count() - 1);
    if (!item)
        return;

    item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));

    QFont font = ui->speechEngineCombo->font();
    font.setBold(true);
    item->setFont(font);
}

void MainWindow::initSpeech()
{
    m_audioPlayer = new AudioPlayer(this);
    m_notificationSound = new NotificationSound(this);

    ui->speechTextCombo->addItem(tr("Translation"), false);
    ui->speechTextCombo->addItem(tr("Original"), true);

    ui->speechSourceCombo->addItem(tr("Any source"), QString());
    ui->speechSourceCombo->addItem(tr("Screen (Tesseract)"), QStringLiteral("Tesseract"));
    ui->speechSourceCombo->addItem(tr("Screen (Ollama Vision)"), QStringLiteral("Ollama Vision"));
    ui->speechSourceCombo->addItem(tr("Clipboard"), QStringLiteral("Clipboard"));
    ui->speechSourceCombo->addItem(QStringLiteral("Hook"), QStringLiteral("Hook"));

    ui->speechTranslatorCombo->addItem(tr("Whichever answers first"), QString());
    ui->speechTranslatorCombo->addItem(QStringLiteral("Google"), QStringLiteral("Google"));
    ui->speechTranslatorCombo->addItem(QStringLiteral("Ollama"), QStringLiteral("Ollama"));

    ui->speechModeCombo->addItem(tr("Automatic"), false);
    ui->speechModeCombo->addItem(tr("Manual (hotkey or button)"), true);

    ui->speechOnNewTranslationCombo->addItem(tr("Interrupt the current phrase"), true);
    ui->speechOnNewTranslationCombo->addItem(tr("Let the current phrase finish"), false);

    ui->speechTestButton->setToolTip(tr("Speaks with the settings as applied. Press Apply first to hear a change."));

    ui->speechEngineSettingsButton->setToolTip(tr("Settings for the chosen engine"));
    ui->speechEngineSettingsButton->setAccessibleName(tr("Engine settings"));

    PiperTts *piper = new PiperTts(this);
    registerTtsEngine(piper, {piper->id(), piper->name(),
        {QStringLiteral("piper-tts[http]")},
                      {QStringLiteral("piper"), QStringLiteral("flask")},
                      {}});

    EdgeTts *edge = new EdgeTts(this);
    registerTtsEngine(edge, {edge->id(), edge->name(),
                             {QStringLiteral("edge-tts"), QStringLiteral("flask")},
                             {QStringLiteral("edge_tts"), QStringLiteral("flask")},
                             tr("Edge TTS speaks through a Microsoft service rather than on "
                                "this machine, so every phrase - translated or original - is "
                                "sent over the internet, and nothing is spoken without a "
                                "connection. The download itself is a small one.")});

    CustomTts *custom = new CustomTts(this);
    registerTtsEngine(custom, {});

    populateSpeechEngines();
    setTtsEngine(piper);

    connect(m_audioPlayer, &AudioPlayer::errorOccurred, this, [this](const QString &error) {
        ui->speechStatusLabel->setText(error);
        refreshSpeechPage();
    });
    connect(m_audioPlayer, &AudioPlayer::playingChanged, this, [this](bool playing) {
        refreshSpeechPage();

        if (playing || m_speechPending.isEmpty())
            return;

        const QString text = m_speechPending;
        m_speechPending.clear();

        if (m_speaking)
            m_speaking->synthesize(replaceText(QStringLiteral("Speech"), text));
    });

    connect(ui->speechVoiceCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const QString key = ui->speechVoiceCombo->currentData().toString();
        if (!key.isEmpty() && key != m_tts->voice())
            markSpeechChanged();
    });

    connect(ui->speechSpeedSpin, &QDoubleSpinBox::valueChanged, this, [this](double multiplier) {
        const int percent = qRound(multiplier * 100.0);
        if (percent == m_tts->speed())
            return;

        m_tts->setSpeed(percent);
        markSpeechChanged();
    });

    connect(ui->speechVolumeSlider, &QSlider::valueChanged, this, [this](int percent) {
        ui->speechVolumeValueLabel->setText(QStringLiteral("%1 %").arg(percent));
        m_audioPlayer->setVolume(percent);
    });

    connect(m_pythonController, &PythonController::jobFinished, this,
            [this](const QString &id, bool ok, const QString &) {
                if (id != m_tts->id())
                    return;

                if (!ok || !m_pythonController->componentReady(id)) {
                    stopSpeech();
                    disableSpeech();
                    return;
                }

                if (m_speechOn)
                    startSpeech();
            });

    connect(m_translationController, &TranslationController::translationReady, this,
            [this](const QString &source, const QString &translatorName,
                   const QString &original, const QString &translated) {
                if (translated.isEmpty())
                    return;

                if (ui->speechTextCombo->currentData().toBool())
                    return;

                if (!speechAcceptsSource(source))
                    return;

                const QString wanted = ui->speechTranslatorCombo->currentData().toString();
                if (wanted.isEmpty()) {
                    if (m_speechSpokenOriginals.value(source) == original)
                        return;

                    m_speechSpokenOriginals.insert(source, original);
                } else if (translatorName != wanted) {
                    return;
                }
            offerSpeech(translated);
        });

    connect(m_translationController, &TranslationController::originalReady, this,
            [this](const QString &source, const QString &original, bool) {
                if (original.isEmpty())
                    return;

                if (!ui->speechTextCombo->currentData().toBool())
                    return;

                if (!speechAcceptsSource(source))
                    return;

                offerSpeech(original);
            });
}

void MainWindow::refreshSpeechPage()
{
    QString status;
    switch (m_tts->state()) {
        case TtsEngine::State::Stopped:
            status = tr("Not running");
            break;
        case TtsEngine::State::Starting:
            status = tr("Starting...");
            break;
        case TtsEngine::State::Ready:
            status = m_tts->voice().isEmpty()
            ? tr("Ready at %1, no voice chosen yet").arg(m_tts->baseUrl())
            : tr("Ready at %1, voice %2")
            .arg(m_tts->baseUrl(), m_tts->voiceLabel(m_tts->voice()));
            break;
        case TtsEngine::State::Failed:
            status = tr("Failed");
            break;
    }

    if (m_speaking && m_speaking != m_tts)
        status = tr("Not applied yet — %1 is still in use").arg(m_speaking->name());

    ui->speechStatusLabel->setText(status);
    ui->speechEngineNote->setText(m_tts->summary());
    ui->speechEngineSettingsButton->setEnabled(m_tts->hasSettings());
    ui->speechTestButton->setEnabled(m_tts->state() == TtsEngine::State::Ready);
    ui->speechStopButton->setEnabled(speechBusy());
    ui->speechTranslatorCombo->setEnabled(!ui->speechTextCombo->currentData().toBool());
    ui->speechOnNewTranslationCombo->setEnabled(!speechManual());

    if (m_outputWindow)
        m_outputWindow->setSpeechBusy(speechBusy());
}

void MainWindow::refreshSpeechVoices(VoiceToShow show)
{
    const QSignalBlocker blocker(ui->speechVoiceCombo);
    const QStringList voices = m_tts->availableVoices();

    const QString shownNow = show == VoiceToShow::Pending
    ? ui->speechVoiceCombo->currentData().toString()
    : QString();

    const QString wanted = shownNow.isEmpty() ? m_tts->voice() : shownNow;

    QList<QPair<QString, QString>> shown;
    shown.reserve(voices.size());

    for (const QString &key : voices)
        shown.append({m_tts->voiceLabel(key), key});

    std::sort(shown.begin(), shown.end(),
              [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
                  return a.first.localeAwareCompare(b.first) < 0;
              });

    ui->speechVoiceCombo->clear();
    for (const QPair<QString, QString> &voice : std::as_const(shown))
        ui->speechVoiceCombo->addItem(voice.first, voice.second);

    int index = ui->speechVoiceCombo->findData(wanted);

    if (index < 0 && wanted != m_tts->voice())
        index = ui->speechVoiceCombo->findData(m_tts->voice());

    if (index >= 0) {
        ui->speechVoiceCombo->setCurrentIndex(index);
    } else if (!shown.isEmpty()) {
        const QString had = m_tts->voice();
        const QString replacement = shown.first().second;

        ui->speechVoiceCombo->setCurrentIndex(0);
        setSpeechVoice(replacement);

        if (!had.isEmpty())
            storeSpeechVoice(replacement);
    }

    ui->speechVoiceCombo->setEnabled(!voices.isEmpty());
    ui->speechVoiceCombo->setToolTip(
        !voices.isEmpty()               ? QString()
        : !m_tts->needsVoiceToStart()   ? tr("The list comes from the service; switch speech on "
                                             "and apply once to read it.")
        : m_tts->useExternalServer()    ? tr("The server has not reported its voices yet.")
        : tr("Open the engine settings to download a voice."));
}

void MainWindow::refreshSpeechSpeed()
{
    const QSignalBlocker blocker(ui->speechSpeedSpin);
    ui->speechSpeedSpin->setValue(m_tts->speed() / 100.0);
}

void MainWindow::markSpeechChanged()
{
    m_speechChanged = true;
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void MainWindow::setSpeechVoice(const QString &key)
{
    if (key == m_tts->voice())
        return;

    m_tts->setVoice(key);

    ui->speechTestEdit->setText(m_tts->sampleText());
}

void MainWindow::storeSpeechVoice(const QString &key)
{
    QJsonObject speech = Config::getValue("speech").toJsonObject();
    QJsonObject settings = speech.value(m_tts->id()).toObject();

    if (settings.value(QStringLiteral("voice")).toString() == key)
        return;

    settings["voice"] = key;
    speech[m_tts->id()] = settings;

    Config::setValue("speech", speech);
    Config::save();
}

void MainWindow::startSpeech()
{
    if (m_speaking && m_speaking != m_tts)
        stopSpeech();

    if (m_tts->useExternalServer()) {
        m_speaking = m_tts;
        m_tts->start();
        return;
    }

    if (!m_pythonController->componentReady(m_tts->id())) {
        m_pythonController->installComponent(m_tts->id());
        return;
    }

    if (m_tts->needsVoiceToStart() && m_tts->availableVoices().isEmpty()) {
        DialogUtils::information(this, m_tts->name(),
                                 tr("Open the engine settings and download a voice first."));
        disableSpeech();
        return;
    }

    m_speaking = m_tts;
    m_tts->start();
}

void MainWindow::stopSpeech()
{
    m_speechPending.clear();
    m_speechSpokenOriginals.clear();
    m_audioPlayer->stop();

    if (!m_speaking)
        return;

    m_speaking->stop();
    m_speaking = nullptr;
}

void MainWindow::disableSpeech()
{
    {
        const QSignalBlocker blocker(ui->speechEnabled);
        ui->speechEnabled->setChecked(false);
    }

    m_speechOn = false;

    if (m_outputWindow)
        m_outputWindow->setSpeechEnabled(false);

    QJsonObject speech = Config::getValue("speech").toJsonObject();
    if (!speech["is_speech"].toBool())
        return;

    speech["is_speech"] = false;
    Config::setValue("speech", speech);
    Config::save();
}

void MainWindow::speak(const QString &text)
{
    if (!m_speaking)
        return;

    const bool interrupt = ui->speechOnNewTranslationCombo->currentData().toBool();

    if (!interrupt && speechBusy()) {
        m_speechPending = text;
        return;
    }

    m_speechPending.clear();
    m_audioPlayer->stop();
    m_speaking->synthesize(replaceText(QStringLiteral("Speech"), text));

    refreshSpeechPage();
}

void MainWindow::offerSpeech(const QString &text)
{
    m_speechLastText = text;

    if (!m_speechOn || speechManual())
        return;

    speak(text);
}

void MainWindow::toggleSpeech(bool announce)
{
    if (m_speechOn) {
        stopSpeech();
        disableSpeech();
        refreshSpeechPage();

        if (announce && ui->speechToggleSoundCheckBox->isChecked())
            m_notificationSound->playDisabled();
        return;
    }

    {
        const QSignalBlocker blocker(ui->speechEnabled);
        ui->speechEnabled->setChecked(true);
    }

    m_speechOn = true;

    if (m_outputWindow)
        m_outputWindow->setSpeechEnabled(true);

    QJsonObject speech = Config::getValue("speech").toJsonObject();
    if (!speech["is_speech"].toBool()) {
        speech["is_speech"] = true;
        Config::setValue("speech", speech);
        Config::save();
    }

    const bool current = m_speaking == m_tts && (m_tts->state() == TtsEngine::State::Ready
                                              || m_tts->state() == TtsEngine::State::Starting);

    if (!current)
        startSpeech();

    refreshSpeechPage();

    if (announce && ui->speechToggleSoundCheckBox->isChecked())
        m_notificationSound->playEnabled();

    maybeSuggestLanguagePreset();
}

void MainWindow::speakLastText()
{
    if (!m_speaking || m_speechLastText.isEmpty())
        return;

    m_speechPending.clear();
    m_audioPlayer->stop();
    m_speaking->synthesize(replaceText(QStringLiteral("Speech"), m_speechLastText));

    refreshSpeechPage();
}

bool MainWindow::speechManual() const
{
    return ui->speechModeCombo->currentData().toBool();
}

bool MainWindow::speechBusy() const
{
    return (m_speaking && m_speaking->synthesizing())
    || (m_tts && m_tts->synthesizing())
    || m_audioPlayer->isPlaying();
}

bool MainWindow::speechAcceptsSource(const QString &source) const
{
    const QString wanted = ui->speechSourceCombo->currentData().toString();
    if (wanted.isEmpty())
        return true;

    if (wanted == QLatin1String("Hook"))
        return source.startsWith(QLatin1String("Hook"));

    return source == wanted;
}

// ===============================================================
// unsaved changes
// ===============================================================

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
    m_pythonChanged = value;
    m_speechChanged = value;

    for (QObject *w : m_changedWidgets) {
        if (w) w->setProperty("changed", QVariant(value));
    }
}

// ===============================================================
// menus and previews
// ===============================================================

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

// ===============================================================
// overlay
// ===============================================================

void MainWindow::captureRegion()
{
    if (m_overlayWindow->isHidden() && (!m_isShortcuts || ui->generalRadioHotKey->isChecked())) {
        m_isShortcuts = true;

        if (m_overlayImage.isNull()) {
            DialogUtils::warning(this, tr("Warning"), tr("No screencast selected for OCR"));
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

// ===============================================================
// screen casting
// ===============================================================

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
                Config::save();
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

// ===============================================================
// hook
// ===============================================================

void MainWindow::flushHookBurst()
{
    if (m_hookBurstBuffer.isEmpty())
        return;

    const QStringList winners = m_outputWindow->hookSourcesToOutput(m_hookBurstBuffer.keys());
    QStringList sent;
    for (const QString &src : winners)
        if (m_hookBurstBuffer.contains(src))
            sent << src;

    m_outputWindow->beginHookBatch(sent);

    for (const QString &src : sent)
        m_translationController->translate(src, m_hookBurstBuffer.value(src));

    m_hookBurstBuffer.clear();
}

// ===============================================================
// preset [speech]
// ===============================================================

QString MainWindow::presetsDirPath() const
{
    return Config::getConfigDirPath() + QStringLiteral("presets/");
}

static int presetVersion(const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly))
        return -1;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object().value(QStringLiteral("version")).toInt(0) : 0;
}

void MainWindow::ensurePresetsSeed()
{
    QDir dir(presetsDirPath());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const QString seeded = dir.filePath(QStringLiteral("tts_ru.json"));
    const QString builtin = QStringLiteral(":/presets/tts_ru.json");

    if (QFileInfo::exists(seeded)) {
        if (presetVersion(builtin) <= presetVersion(seeded)) {
            return;
        }
        QFile::remove(seeded);
    }

    if (QFile::copy(builtin, seeded)) {
        QFile::setPermissions(seeded, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    } else {
        Log(Logger::Level::Warning, QStringLiteral("[text] could not seed built-in preset into ") + seeded);
    }
}

void MainWindow::setupReplacementPresets()
{
    ensurePresetsSeed();

    QMenu *menu = new QMenu(this);
    connect(menu, &QMenu::aboutToShow, this, &MainWindow::rebuildPresetMenu);
    ui->textProcessingPresetButton->setMenu(menu);
}

void MainWindow::rebuildPresetMenu()
{
    QMenu *menu = ui->textProcessingPresetButton->menu();
    if (!menu)
        return;

    menu->clear();

    const QFileInfoList files = QDir(presetsDirPath()).entryInfoList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);

    for (const QFileInfo &info : files) {
        const QString path = info.absoluteFilePath();
        QString name;
        QJsonArray rules;
        const QString label = readPresetFile(path, name, rules) ? name : info.completeBaseName();
        menu->addAction(label, this, [this, path] { choosePresetTarget(path); });
    }

    if (files.isEmpty())
        menu->addAction(tr("No presets found"))->setEnabled(false);

    menu->addSeparator();
    menu->addAction(tr("Load from file…"), this, &MainWindow::importPresetFile);
    menu->addAction(tr("Export current profile…"), this, &MainWindow::exportCurrentProfile);
    menu->addAction(tr("Open presets folder"), this, &MainWindow::openPresetsFolder);
}

bool MainWindow::readPresetFile(const QString &path, QString &name, QJsonArray &rules, QString *lang)
{
    if (lang)
        lang->clear();

    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        Log(Logger::Level::Warning, QStringLiteral("[text] preset: cannot open %1").arg(path));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        Log(Logger::Level::Warning, QStringLiteral("[text] preset %1: invalid JSON (%2)").arg(path, parseError.errorString()));
        return false;
    }

    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        name = obj["name"].toString();
        rules = obj["rules"].toArray();
        if (lang) {
            *lang = obj["lang"].toString();
        }
    } else if (doc.isArray()) {
        name.clear();
        rules = doc.array();
    } else {
        return false;
    }

    if (name.isEmpty())
        name = QFileInfo(path).completeBaseName();

    return true;
}

void MainWindow::choosePresetTarget(const QString &path)
{
    QString presetName;
    QJsonArray rules;
    if (!readPresetFile(path, presetName, rules) || rules.isEmpty()) {
        QMessageBox::warning(this, tr("Load preset"), tr("This preset file could not be read."));
        return;
    }

    const QString currentLabel = m_activeProfile.isEmpty() ? tr("Default") : m_activeProfile;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Load preset"));
    box.setText(tr("Load preset \"%1\".").arg(presetName));
    box.setInformativeText(tr("Load it into a new profile, or add it to the current profile \"%1\"?").arg(currentLabel));
    QPushButton *newBtn = box.addButton(tr("New profile"), QMessageBox::AcceptRole);
    QPushButton *curBtn = box.addButton(tr("Add to current"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(newBtn);
    box.exec();

    QAbstractButton *clicked = box.clickedButton();
    if (clicked == newBtn) {
        applyReplacementPresetFile(path, false);
    } else if (clicked == curBtn) {
        applyReplacementPresetFile(path, true);
    }
}

void MainWindow::applyReplacementPresetFile(const QString &path, bool intoCurrentProfile, bool commitNow)
{
    QString presetName;
    QJsonArray rules;
    if (!readPresetFile(path, presetName, rules)) {
        QMessageBox::warning(this, tr("Load preset"), tr("This preset file could not be read."));
        return;
    }

    const QString targetProfile = intoCurrentProfile ? m_activeProfile : presetName;
    if (!intoCurrentProfile) {
        if (!m_ruleProfiles.contains(targetProfile)) {
            m_ruleProfiles << targetProfile;
        }
        m_replacementRules.erase(std::remove_if(m_replacementRules.begin(),
                                                m_replacementRules.end(),
                                                [&targetProfile](const ReplacementRule &r) {
                                                    return r.profile == targetProfile;
                                                }), m_replacementRules.end());
    }

    for (const QJsonValue &value : std::as_const(rules)) {
        const QJsonObject rowObject = value.toObject();

        ReplacementRule rule;
        rule.regex = rowObject["regex"].toBool();
        rule.source = rowObject["source"].toString();
        rule.from = rowObject["from"].toString();
        rule.to = rowObject["to"].toString();
        rule.profile = targetProfile;

        const bool exists = std::any_of(m_replacementRules.cbegin(),
                                        m_replacementRules.cend(),
                                        [&rule](const ReplacementRule &existing) {
                                            return existing.regex == rule.regex && existing.source == rule.source
                                                   && existing.from == rule.from && existing.to == rule.to
                                                   && existing.profile == rule.profile;
                                        });

        if (exists)
            continue;

        m_replacementRules << rule;
    }

    m_activeProfile = targetProfile;
    refreshProfileBox();

    if (commitNow) {
        m_liveProfile = m_activeProfile;
        persistProfilesMeta();
        persistReplacementRules();
    } else {
        persistProfilesMeta();
        markRulesChanged();
    }
}

void MainWindow::importPresetFile()
{
    const QString src = QFileDialog::getOpenFileName(this, tr("Load preset"), QString(), tr("Preset files (*.json)"));

    if (src.isEmpty())
        return;

    QString name;
    QJsonArray rules;
    if (!readPresetFile(src, name, rules) || rules.isEmpty()) {
        QMessageBox::warning(this, tr("Load preset"), tr("This preset file could not be read."));
        return;
    }

    QDir dir(presetsDirPath());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const QFileInfo srcInfo(src);
    if (srcInfo.absolutePath() == QFileInfo(dir.path()).absoluteFilePath()) {
        choosePresetTarget(src);
        return;
    }

    QString dest = dir.filePath(srcInfo.fileName());
    for (int n = 1; QFileInfo::exists(dest); ++n)
        dest = dir.filePath(srcInfo.completeBaseName() + QStringLiteral("_%1.").arg(n) + srcInfo.suffix());

    if (!QFile::copy(src, dest)) {
        QMessageBox::warning(this, tr("Load preset"), tr("The preset could not be copied into the presets folder."));
        return;
    }
    QFile::setPermissions(dest, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

    choosePresetTarget(dest);
}

void MainWindow::exportCurrentProfile()
{
    commitReplacementTable();

    const QString profileName = m_activeProfile.isEmpty() ? tr("Default") : m_activeProfile;

    QJsonArray rules;
    for (const ReplacementRule &rule : std::as_const(m_replacementRules)) {
        if (rule.profile != m_activeProfile)
            continue;

        QJsonObject row;
        row["regex"] = rule.regex;
        row["source"] = rule.source;
        row["from"] = rule.from;
        row["to"] = rule.to;
        rules.append(row);
    }

    if (rules.isEmpty()) {
        QMessageBox::information(this, tr("Export preset"), tr("This profile has no rules to export."));
        return;
    }

    QDir dir(presetsDirPath());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QString suggested = profileName;
    suggested.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
    if (suggested.isEmpty())
        suggested = QStringLiteral("preset");

    const QString path = QFileDialog::getSaveFileName(this, tr("Export preset"),
        dir.filePath(suggested + QStringLiteral(".json")), tr("Preset files (*.json)"));
    if (path.isEmpty())
        return;

    QJsonObject root;
    root["name"] = profileName;
    root["rules"] = rules;

    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        QMessageBox::warning(this, tr("Export preset"), tr("The preset could not be written."));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

    Log(Logger::Level::Info, QStringLiteral("[text] exported profile '%1' (%2 rule(s)) to %3").arg(profileName).arg(rules.size()).arg(path));
}

void MainWindow::openPresetsFolder()
{
    QDir dir(presetsDirPath());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QDesktopServices::openUrl(QUrl::fromLocalFile(dir.path()));
}

void MainWindow::persistOfferedPresetLangs()
{
    QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();

    QJsonArray langs;
    QStringList sorted = m_offeredPresetLangs.values();
    sorted.sort();
    for (const QString &lang : std::as_const(sorted))
        langs.append(lang);

    if (textProcessing["offered_preset_langs"].toArray() == langs)
        return;

    textProcessing["offered_preset_langs"] = langs;
    Config::setValue("text_processing", textProcessing);
    Config::save();
}

QString MainWindow::currentSpeechLang() const
{
    if (!m_tts) return QString();
    return m_tts->voiceLanguage(m_tts->voice());
}

void MainWindow::maybeSuggestLanguagePreset()
{
    if (m_suggestingPreset)
        return;

    if (!m_speechOn)
        return;

    if (!ui->speechOfferPresetsCheckBox->isChecked())
        return;

    const QString lang = currentSpeechLang();
    if (lang.isEmpty() || m_offeredPresetLangs.contains(lang))
        return;

    QString presetPath;
    QString presetName;

    const QFileInfoList files = QDir(presetsDirPath()).entryInfoList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);

    for (const QFileInfo &info : files) {
        QString name;
        QString fileLang;
        QJsonArray rules;
        if (readPresetFile(info.absoluteFilePath(), name, rules, &fileLang)
            && !fileLang.isEmpty() && fileLang.compare(lang, Qt::CaseInsensitive) == 0) {
            presetPath = info.absoluteFilePath();
            presetName = name;
            break;
        }
    }

    if (presetPath.isEmpty())
        return;

    const QString currentLabel = m_activeProfile.isEmpty() ? tr("Default") : m_activeProfile;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("TTS preset"));
    box.setText(tr("A ready-made TTS preset \"%1\" is available.").arg(presetName));
    box.setInformativeText(tr("It adds text-normalization rules under Text processing → "
                              "String replacement, where you can also load it yourself later."
                              "\n\nLoad it into a new profile, or add it to "
                              "the current profile \"%1\"?").arg(currentLabel));

    QPushButton *newBtn = box.addButton(tr("New profile"), QMessageBox::AcceptRole);
    QPushButton *curBtn = box.addButton(tr("Add to current"), QMessageBox::AcceptRole);
    box.addButton(tr("Not now"), QMessageBox::RejectRole);
    box.setDefaultButton(newBtn);
    box.setWindowModality(Qt::WindowModal);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);

    m_suggestingPreset = true;
    box.show();
    box.raise();
    box.activateWindow();
    box.exec();
    m_suggestingPreset = false;

    QAbstractButton *clicked = box.clickedButton();
    if (clicked == newBtn) {
        applyReplacementPresetFile(presetPath, false, true);
    } else if (clicked == curBtn) {
        applyReplacementPresetFile(presetPath, true, true);
    }

    m_offeredPresetLangs.insert(lang);
    persistOfferedPresetLangs();
}

// ===============================================================
// text replacement
// ===============================================================

QTableWidgetItem *MainWindow::makeRegexFlagItem(bool checked)
{
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QString MainWindow::replaceText(const QString &source, QString output)
{
    for (const ReplacementRule &rule : std::as_const(m_replacementRules)) {
        if (rule.profile != m_liveProfile)
            continue;
        if (rule.from.isEmpty())
            continue;
        if (!rule.source.isEmpty() && rule.source.compare(source, Qt::CaseInsensitive) != 0)
            continue;

        if (rule.regex) {
            QRegularExpression re(rule.from);
            if (re.isValid())
                output.replace(re, rule.to);
        } else {
            output.replace(rule.from, rule.to);
        }
    }

    return output;
}

void MainWindow::markRulesChanged()
{
    m_textProcessingChanged = true;
    ui->textProcessingTableWidget->setProperty("changed", true);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

// ===============================================================
// profiles and bindings
// ===============================================================

void MainWindow::saveProfileBindings()
{
    QJsonObject bindings;
    for (auto it = m_profileBindings.constBegin(); it != m_profileBindings.constEnd(); ++it) {
        if (it.value().isEmpty())
            continue;
        QStringList targets = it.value().values();
        targets.sort();
        QJsonArray arr;
        for (const QString &target : std::as_const(targets)) {
            arr.append(target);
        }
        bindings[it.key()] = arr;
    }

    QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();
    if (textProcessing["profile_bindings"].toObject() == bindings)
        return;

    textProcessing["profile_bindings"] = bindings;
    Config::setValue("text_processing", textProcessing);
    Config::save();
}

QString MainWindow::activeHookTargetKey() const
{
    const QString plugin = m_hookController->currentRunningPlugin();
    if (plugin.isEmpty())
        return QString();

    QString process = m_hookController->runningEngineProcess();
    if (process.isEmpty()) {
        for (const auto &p : m_registry) {
            if (p.name == plugin) {
                process = p.targetExecutable;
                break;
            }
        }
    }
    return plugin + QStringLiteral("|") + process;
}

QString MainWindow::currentGameLabel() const
{
    const QString plugin = m_hookController->currentRunningPlugin();

    if (plugin.isEmpty())
        return QString();

    const QString process = m_hookController->runningEngineProcess();
    if (!process.isEmpty())
        return process;

    for (const auto &p : m_registry) {
        if (p.name == plugin) {
            return p.targetTitle.isEmpty() ? p.name : p.targetTitle;
        }
    }
    return plugin;
}

QString MainWindow::hookTargetLabel(const QString &key) const
{
    const QString plugin = key.section(QLatin1Char('|'), 0, 0);
    const QString process = key.section(QLatin1Char('|'), 1);
    for (const auto &p : m_registry) {
        if (p.name != plugin) {
            continue;
        }
        if ((p.category == QLatin1String("game") || p.category == QLatin1String("application")) && !p.targetTitle.isEmpty()) {
            return QStringLiteral("%1 (%2)").arg(p.targetTitle, p.name);
        }
        break;
    }

    if (process.isEmpty())
        return plugin;
    return QStringLiteral("%1 (%2)").arg(process, plugin);
}

void MainWindow::refreshBindingBox()
{
    QComboBox *box = ui->textProcessingBindingBox;
    const QSignalBlocker blocker(box);

    box->clear();

    QStringList targets = m_profileBindings.value(m_activeProfile).values();
    targets.sort();
    if (targets.isEmpty()) {
        box->addItem(tr("None"), QString());
    } else {
        for (const QString &key : std::as_const(targets)) {
            box->addItem(hookTargetLabel(key), key);
        }
    }

    const QString target = activeHookTargetKey();
    const int currentIdx = target.isEmpty() ? -1 : box->findData(target);
    box->setCurrentIndex(currentIdx >= 0 ? currentIdx : 0);

    const bool canAdd = !target.isEmpty() && !m_profileBindings.value(m_activeProfile).contains(target);

    ui->textProcessingBindingAddButton->setEnabled(canAdd);
    ui->textProcessingBindingRemoveButton->setEnabled(!box->currentData().toString().isEmpty());
}

void MainWindow::activateProfileForHook()
{
    const QString target = activeHookTargetKey();

    bool found = false;
    QString bound;
    if (!target.isEmpty()) {
        QStringList candidates = m_ruleProfiles;
        candidates.append(QString());
        for (const QString &profile : std::as_const(candidates)) {
            if (m_profileBindings.value(profile).contains(target)) {
                found = true;
                bound = profile;
                break;
            }
        }
    }

    if (target != m_lastHookTarget) {
        // A different game (or none) is running now - any earlier manual
        // pick was scoped to the previous game, so it no longer applies
        if (m_profileHookState == ProfileHookState::ManualOverride)
            m_profileHookState = ProfileHookState::Idle;
        m_lastHookTarget = target;
    }

    if (found && m_profileHookState != ProfileHookState::ManualOverride) {
        if (bound != m_liveProfile) {
            if (m_profileHookState != ProfileHookState::AutoSwitched) {
                m_profileBeforeHook = m_liveProfile;
                m_profileHookState = ProfileHookState::AutoSwitched;
            }
            m_liveProfile = bound;
            m_activeProfile = bound;
            refreshProfileBox();
            return;
        }
    } else if (!found && m_profileHookState == ProfileHookState::AutoSwitched) {
        m_profileHookState = ProfileHookState::Idle;
        if (m_profileBeforeHook != m_liveProfile) {
            m_liveProfile = m_profileBeforeHook;
            m_activeProfile = m_profileBeforeHook;
            refreshProfileBox();
            return;
        }
    }

    refreshBindingBox();
}

void MainWindow::discardPendingProfileOverride()
{
    if (m_profileHookState == ProfileHookState::ManualOverride)
        m_profileHookState = ProfileHookState::Idle;
}

void MainWindow::addProfileBinding()
{
    const QString target = activeHookTargetKey();
    if (target.isEmpty())
        return;

    for (auto it = m_profileBindings.begin(); it != m_profileBindings.end(); ++it) {
        if (it.key() == m_activeProfile || !it.value().contains(target))
            continue;

        const QString other = it.key().isEmpty() ? tr("Default") : it.key();
        const auto reply = QMessageBox::question(this,
                                                 tr("Bind current game"),
                                                 tr("\"%1\" already auto-selects the profile \"%2\". Move it to this profile?")
                                                 .arg(currentGameLabel(), other));
        if (reply != QMessageBox::Yes)
            return;

        it.value().remove(target);

        if (it.value().isEmpty()) {
            m_profileBindings.erase(it);
        }

        break;
    }

    m_profileBindings[m_activeProfile].insert(target);
    markRulesChanged();
    refreshBindingBox();
}

void MainWindow::removeProfileBinding()
{
    const QString target = ui->textProcessingBindingBox->currentData().toString();
    if (target.isEmpty())
        return;

    QSet<QString> &binding = m_profileBindings[m_activeProfile];
    binding.remove(target);
    if (binding.isEmpty()) {
        m_profileBindings.remove(m_activeProfile);
    }

    markRulesChanged();
    refreshBindingBox();
}

void MainWindow::refreshProfileBox()
{
    QComboBox *box = ui->textProcessingProfileBox;
    const QSignalBlocker blocker(box);

    box->clear();
    box->addItem(tr("Default"), QString());
    for (const QString &name : std::as_const(m_ruleProfiles))
        box->addItem(name, name);

    int idx = box->findData(m_activeProfile);
    if (idx < 0) {
        idx = 0;
        m_activeProfile.clear();
    }
    box->setCurrentIndex(idx);

    const bool custom = !m_activeProfile.isEmpty();
    ui->textProcessingProfileRenameButton->setEnabled(custom);
    ui->textProcessingProfileDeleteButton->setEnabled(custom);

    refreshBindingBox();
    populateReplacementTable();
}

void MainWindow::persistProfilesMeta()
{
    QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();

    QJsonArray profiles;
    for (const QString &name : std::as_const(m_ruleProfiles))
        profiles.append(name);

    if (textProcessing["profiles"].toArray() == profiles && textProcessing["active_profile"].toString() == m_liveProfile)
        return;

    textProcessing["profiles"] = profiles;
    textProcessing["active_profile"] = m_liveProfile;
    Config::setValue("text_processing", textProcessing);
    Config::save();
}

void MainWindow::persistReplacementRules()
{
    QJsonObject textProcessing = Config::getValue("text_processing").toJsonObject();

    QJsonArray jsonArray;
    for (const ReplacementRule &rule : std::as_const(m_replacementRules)) {
        QJsonObject rowObject;
        rowObject["regex"] = rule.regex;
        rowObject["source"] = rule.source;
        rowObject["from"] = rule.from;
        rowObject["to"] = rule.to;
        if (!rule.profile.isEmpty())
            rowObject["profile"] = rule.profile;
        jsonArray.append(rowObject);
    }
    textProcessing["text_replacement_table"] = jsonArray;

    Config::setValue("text_processing", textProcessing);
    Config::save();
}

void MainWindow::addRuleProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New profile"), tr("Profile name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    if (name.compare(tr("Default"), Qt::CaseInsensitive) == 0 || m_ruleProfiles.contains(name)) {
        QMessageBox::warning(this, tr("New profile"), tr("A profile with this name already exists."));
        return;
    }

    m_ruleProfiles << name;
    m_activeProfile = name;
    if (m_profileHookState == ProfileHookState::AutoSwitched)
        m_profileHookState = ProfileHookState::Idle;
    refreshProfileBox();
    persistProfilesMeta();
    markRulesChanged();
}

void MainWindow::renameRuleProfile()
{
    if (m_activeProfile.isEmpty())
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename profile"), tr("Profile name:"), QLineEdit::Normal, m_activeProfile, &ok).trimmed();
    if (!ok || name.isEmpty() || name == m_activeProfile)
        return;

    if (name.compare(tr("Default"), Qt::CaseInsensitive) == 0 || m_ruleProfiles.contains(name)) {
        QMessageBox::warning(this, tr("Rename profile"), tr("A profile with this name already exists."));
        return;
    }

    const QString previous = m_activeProfile;
    for (ReplacementRule &rule : m_replacementRules)
        if (rule.profile == previous)
            rule.profile = name;

    const int i = m_ruleProfiles.indexOf(previous);
    if (i >= 0) {
        m_ruleProfiles[i] = name;
    }

    if (m_profileBindings.contains(previous))
        m_profileBindings.insert(name, m_profileBindings.take(previous));

    m_activeProfile = name;

    refreshProfileBox();
    persistProfilesMeta();
    saveProfileBindings();
    markRulesChanged();
}

void MainWindow::deleteRuleProfile()
{
    if (m_activeProfile.isEmpty())
        return;

    const QString name = m_activeProfile;
    const auto reply = QMessageBox::question(this, tr("Delete profile"), tr("Delete profile \"%1\" and all its rules?").arg(name));
    if (reply != QMessageBox::Yes)
        return;

    m_replacementRules.erase( std::remove_if(m_replacementRules.begin(),
                                            m_replacementRules.end(),
                                            [&name](const ReplacementRule &rule) {
                                                return rule.profile == name;
                                            }), m_replacementRules.end());

    m_ruleProfiles.removeAll(name);
    m_profileBindings.remove(name);
    m_activeProfile.clear();

    refreshProfileBox();
    persistProfilesMeta();
    saveProfileBindings();
    markRulesChanged();
}

void MainWindow::populateReplacementTable()
{
    QTableWidget *table = ui->textProcessingTableWidget;

    // Refilling the table is not a user edit: no commit, no Apply button
    const QSignalBlocker blocker(table);
    table->setRowCount(0);

    for (const ReplacementRule &rule : std::as_const(m_replacementRules)) {
        if (rule.profile != m_activeProfile)
            continue;

        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, ColRegex, makeRegexFlagItem(rule.regex));
        table->setItem(row, ColSource, new QTableWidgetItem(rule.source));
        table->setItem(row, ColFrom, new QTableWidgetItem(rule.from));
        table->setItem(row, ColTo, new QTableWidgetItem(rule.to));
    }
}

void MainWindow::commitReplacementTable()
{
    QTableWidget *table = ui->textProcessingTableWidget;

    QList<ReplacementRule> kept;
    for (const ReplacementRule &rule : std::as_const(m_replacementRules))
        if (rule.profile != m_activeProfile)
            kept << rule;

    for (int i = 0; i < table->rowCount(); ++i) {
        QTableWidgetItem *regexItem = table->item(i, ColRegex);
        QTableWidgetItem *srcItem = table->item(i, ColSource);
        QTableWidgetItem *fromItem = table->item(i, ColFrom);
        QTableWidgetItem *toItem = table->item(i, ColTo);

        ReplacementRule rule;
        rule.regex = regexItem && regexItem->checkState() == Qt::Checked;
        rule.source = srcItem ? srcItem->text().trimmed() : QString();
        rule.from = fromItem ? fromItem->text() : QString();
        rule.to = toItem ? toItem->text() : QString();
        rule.profile = m_activeProfile;
        kept << rule;
    }

    m_replacementRules = kept;
}

// ===============================================================
// config load
// ===============================================================

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
    if (m_pythonChanged)
        loadPythonSettings(Config::getValue("python").toJsonObject());
    if (m_speechChanged)
        loadSpeechSettings(Config::getValue("speech").toJsonObject());

    loadScreencastSettings();
    loadOcrSettings();
    loadHookPluginSettings();

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    setPropertyChanged(false);

    activateProfileForHook();
}

void MainWindow::loadGeneralSettings(const QJsonObject& general)
{
    if (widgetChanged(ui->generalBoxLanguage) && general.contains("language")) {
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

    if (widgetChanged(ui->generalHotkeySelectNewRegionEdit) && general.contains("hotkey_select_region"))
        ui->generalHotkeySelectNewRegionEdit->setKeySequence(QKeySequence(general["hotkey_select_region"].toString()));
    if (widgetChanged(ui->generalHotkeyHistoryTranslationEdit) && general.contains("hotkey_history_translation"))
        ui->generalHotkeyHistoryTranslationEdit->setKeySequence(QKeySequence(general["hotkey_history_translation"].toString()));
    if (widgetChanged(ui->generalHotkeyManualTranslateEdit) && general.contains("hotkey_manual_translate"))
        ui->generalHotkeyManualTranslateEdit->setKeySequence(QKeySequence(general["hotkey_manual_translate"].toString()));
    if (widgetChanged(ui->generalHotkeySpeakTextEdit) && general.contains("hotkey_speak_text"))
        ui->generalHotkeySpeakTextEdit->setKeySequence(QKeySequence(general["hotkey_speak_text"].toString()));
    if (widgetChanged(ui->generalHotkeyStopSpeechEdit) && general.contains("hotkey_stop_speech"))
        ui->generalHotkeyStopSpeechEdit->setKeySequence(QKeySequence(general["hotkey_stop_speech"].toString()));
    if (widgetChanged(ui->generalHotkeyToggleSpeechEdit) && general.contains("hotkey_toggle_speech"))
        ui->generalHotkeyToggleSpeechEdit->setKeySequence(QKeySequence(general["hotkey_toggle_speech"].toString()));

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
        if (widgetChanged(ui->generalHotkeySpeakTextEdit))
            ui->generalHotkeySpeakTextEdit->setEnabled(isX11Mode);
        if (widgetChanged(ui->generalHotkeyStopSpeechEdit))
            ui->generalHotkeyStopSpeechEdit->setEnabled(isX11Mode);
        if (widgetChanged(ui->generalHotkeyToggleSpeechEdit))
            ui->generalHotkeyToggleSpeechEdit->setEnabled(isX11Mode);

        ui->generalBindShortcut->setEnabled(!isX11Mode);
    }
#endif
}

void MainWindow::loadOutputSettings(const QJsonObject& output)
{
    if (widgetChanged(ui->outputToggledOriginalScreencast))
        ui->outputToggledOriginalScreencast->setChecked(output["original_screencast_output"].toBool(true));
    if (widgetChanged(ui->outputToggledProcessedScreencast))
        ui->outputToggledProcessedScreencast->setChecked(output["processed_screencast_output"].toBool(true));
    if (widgetChanged(ui->outputToggledScreencast))
        ui->outputToggledScreencast->setChecked(output["disable_screencast"].toBool(false));
    if (widgetChanged(ui->outputGeneralBoxFramerate))
        ui->outputGeneralBoxFramerate->setCurrentIndex(output["framerate_index"].toInt(2));

    QJsonObject processing = output["processing"].toObject();
    if (widgetChanged(ui->outputProcessedToggledBlur))
        ui->outputProcessedToggledBlur->setChecked(processing["is_blur"].toBool(false));
    if (widgetChanged(ui->outputProcessedBlurType))
        ui->outputProcessedBlurType->setCurrentIndex(processing["blur_type"].toInt(0));
    if (widgetChanged(ui->outputProcessedBlurValue))
        ui->outputProcessedBlurValue->setValue(processing["blur_value"].toInt(21));
    if (widgetChanged(ui->outputProcessedBlurSubtract))
        ui->outputProcessedBlurSubtract->setChecked(processing["is_blurSubtract"].toBool(true));
    if (widgetChanged(ui->outputProcessedBlurNormalize))
        ui->outputProcessedBlurNormalize->setChecked(processing["is_blurNormalize"].toBool(true));
    if (widgetChanged(ui->outputProcessedSimpleThresh))
        ui->outputProcessedSimpleThresh->setChecked(processing["is_simple_thresholding"].toBool(true));
    if (widgetChanged(ui->outputProcessedAdaptiveThresh))
        ui->outputProcessedAdaptiveThresh->setChecked(processing["is_adaptive_thresholding"].toBool(false));
    if (widgetChanged(ui->outputProcessedOtsu))
        ui->outputProcessedOtsu->setChecked(processing["is_otsu_binarization"].toBool(false));
    if (widgetChanged(ui->outputProcessedSimpleThresholdingType))
        ui->outputProcessedSimpleThresholdingType->setCurrentIndex(processing["simple_threshold_type"].toInt(0));
    if (widgetChanged(ui->outputProcessedThreshValue))
        ui->outputProcessedThreshValue->setValue(processing["threshold_value"].toInt(185));
    if (widgetChanged(ui->outputProcessedAdaptiveThresholdingType))
        ui->outputProcessedAdaptiveThresholdingType->setCurrentIndex(processing["adaptive_method"].toInt(0));
}

void MainWindow::loadTranslatorSettings(const QJsonObject& translator)
{
    QJsonObject translator_online = translator["translator_online"].toObject();
    if (!translator_online.isEmpty()) {
        QJsonObject google = translator_online["google"].toObject();
        if (widgetChanged(ui->translatorOnlineGoogleToggled))
            ui->translatorOnlineGoogleToggled->setChecked(google["is_google"].toBool(false));

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
            ui->translatorOfflineOllamaToggled->setChecked(ollama_translator["is_ollama_translator"].toBool(false));

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

    // A translator just switched on has nothing to work with until the next text
    // shows up, so hand it what is on screen right now
    const bool googleTurnedOn = ui->translatorOnlineGoogleToggled->isChecked()
                                && widgetChanged(ui->translatorOnlineGoogleToggled);
    const bool ollamaTurnedOn = ui->translatorOfflineOllamaToggled->isChecked()
                                && widgetChanged(ui->translatorOfflineOllamaToggled);

    if (googleTurnedOn || ollamaTurnedOn)
        retranslateText();
}

void MainWindow::loadTextProcessingSettings(const QJsonObject& textProcessing)
{
    if (!textProcessing.isEmpty()) {

        // OCR
        if (widgetChanged(ui->textProcessingOCREngineToggled))
            ui->textProcessingOCREngineToggled->setChecked(textProcessing["is_ocr"].toBool(false));

        // Tesseract
        QJsonObject tesseract = textProcessing["tesseract"].toObject();
        if (widgetChanged(ui->textProcessingOCREngineTesseractRadio))
            ui->textProcessingOCREngineTesseractRadio->setChecked(tesseract["is_tesseract"].toBool(true));

        m_tesseractActiveLang = tesseract.value("lang").toString();
        m_tesseractUseSystemTessdata = tesseract["is_systemdata"].toBool();
        m_tesseractTessdataPath = tesseract["path_tessdata"].toString(QStringLiteral("./tessdata"));
        m_tesseractMode = tesseract["mode"].toInt();
        m_tesseractAutoInterval = tesseract["delay"].toDouble(1);
        m_tesseractSelectedLang = m_tesseractActiveLang;

        m_ocrController->setTesseractMode(m_tesseractMode);
        m_ocrController->setTesseractAutoInterval(m_tesseractAutoInterval);

        // Ollama Vision
        QJsonObject ollama_vision = textProcessing["ollama_vision"].toObject();
        if (widgetChanged(ui->textProcessingOCREngineOllamaVisionRadio))
            ui->textProcessingOCREngineOllamaVisionRadio->setChecked(ollama_vision["is_vision"].toBool(false));

        m_ollamaVisionPrompt = ollama_vision["prompt"].toString();
        m_ollamaVisionMode = ollama_vision["mode"].toInt(Manual);
        m_ollamaVisionAutoInterval = ollama_vision["delay"].toInt(10);

        // HOOK
        QJsonObject hook = textProcessing["hook"].toObject();
        if (widgetChanged(ui->textProcessingHookCheckBox))
            ui->textProcessingHookCheckBox->setChecked(hook["is_hook"].toBool(false));

        m_hookMode = static_cast<HookSettingsDialog::HookMode>(hook["hook_mode"].toInt());
        m_currentGameAppPlugin = hook["current_game_app_plugin"].toString();
        m_currentEnginePlugin = hook["current_engine_plugin"].toString();
        m_currentEngineProcess = hook["current_engine_process"].toString();

        // Clipboard
        if (widgetChanged(ui->textProcessingClipboardCheckBox)) {
            bool isClipboard = textProcessing["is_clipboard"].toBool(false);
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

        // Per-profile bindings
        m_profileBindings.clear();
        const QJsonObject bindings = textProcessing["profile_bindings"].toObject();
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            QSet<QString> set;
            for (const QJsonValue &value : it.value().toArray())
                set.insert(value.toString());
            m_profileBindings.insert(it.key(), set);
        }

        // Named rule profiles
        m_ruleProfiles.clear();
        for (const QJsonValue &value : textProcessing["profiles"].toArray())
            m_ruleProfiles << value.toString();
        m_activeProfile = textProcessing["active_profile"].toString();
        m_liveProfile = m_activeProfile; // committed == shown until the user edits

        // Target languages
        m_offeredPresetLangs.clear();
        for (const QJsonValue &value : textProcessing["offered_preset_langs"].toArray())
            m_offeredPresetLangs.insert(value.toString());

        const QJsonArray jsonArray = textProcessing["text_replacement_table"].toArray();
        if (widgetChanged(ui->textProcessingTableWidget) && textProcessing.contains("text_replacement_table")) {
            m_replacementRules.clear();
            for (const QJsonValue &value : jsonArray) {
                const QJsonObject rowObject = value.toObject();
                ReplacementRule rule;
                rule.regex = rowObject["regex"].toBool();
                rule.source = rowObject["source"].toString();
                rule.from = rowObject["from"].toString();
                rule.to = rowObject["to"].toString();
                rule.profile = rowObject["profile"].toString();  // absent = default profile
                m_replacementRules << rule;
            }

            refreshProfileBox();
        }
    }
}

void MainWindow::loadProxySettings(const QJsonObject& proxy)
{
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

    const QString type = proxy["type"].toString(QStringLiteral("http"));
    if (widgetChanged(ui->proxyTypeHttp))
        ui->proxyTypeHttp->setChecked(type == "http");
    if (widgetChanged(ui->proxyTypeSocks))
        ui->proxyTypeSocks->setChecked(type == "socks");

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

void MainWindow::loadPythonSettings(const QJsonObject& python)
{
    if (widgetChanged(ui->pythonInterpreterEdit))
        ui->pythonInterpreterEdit->setText(python["interpreter"].toString());

    m_pythonController->setPreferredInterpreter(ui->pythonInterpreterEdit->text());
    if (ui->settingsPages->currentWidget() == ui->pythonPage)
        m_pythonController->ensureDetected();
}

void MainWindow::loadSpeechSettings(const QJsonObject& speech)
{
    const QString spokenBefore = m_tts ? m_tts->voice() : QString();
    for (TtsEngine *engine : m_ttsEngines)
        engine->loadSettings(speech.value(engine->id()).toObject());

    TtsEngine *chosen = ttsEngine(speech.value("engine").toString());
    setTtsEngine(chosen ? chosen : m_ttsEngines.first());

    if (m_tts->voice() != spokenBefore)
        ui->speechTestEdit->setText(m_tts->sampleText());

    auto restore = [](QComboBox *box, const QVariant &value) {
        const int index = box->findData(value);
        box->setCurrentIndex(index >= 0 ? index : 0);
    };

    if (widgetChanged(ui->speechTextCombo))
        restore(ui->speechTextCombo, speech.value("original").toBool(false));

    if (widgetChanged(ui->speechSourceCombo))
        restore(ui->speechSourceCombo, speech.value("source").toString());

    if (widgetChanged(ui->speechTranslatorCombo))
        restore(ui->speechTranslatorCombo, speech.value("translator").toString());

    if (widgetChanged(ui->speechModeCombo))
        restore(ui->speechModeCombo, speech.value("manual").toBool(false));

    if (widgetChanged(ui->speechOnNewTranslationCombo))
        restore(ui->speechOnNewTranslationCombo, speech.value("interrupt").toBool(true));

    m_speechSpokenOriginals.clear();

    refreshSpeechSpeed();
    refreshSpeechVoices(VoiceToShow::Applied);
    refreshSpeechPage();

    if (widgetChanged(ui->speechEnabled))
        ui->speechEnabled->setChecked(speech["is_speech"].toBool());

    if (widgetChanged(ui->speechToggleSoundCheckBox))
        ui->speechToggleSoundCheckBox->setChecked(speech.value("toggle_sound").toBool(true));

    if (widgetChanged(ui->speechOfferPresetsCheckBox))
        ui->speechOfferPresetsCheckBox->setChecked(speech.value("offer_tts_presets").toBool(true));

    if (widgetChanged(ui->speechVolumeSlider))
        ui->speechVolumeSlider->setValue(speech.value("volume").toInt(100));

    ui->speechVolumeValueLabel->setText(QStringLiteral("%1 %").arg(ui->speechVolumeSlider->value()));
    m_audioPlayer->setVolume(ui->speechVolumeSlider->value());

    m_speechOn = ui->speechEnabled->isChecked();

    if (m_outputWindow)
        m_outputWindow->setSpeechEnabled(m_speechOn);

    if (!m_speechOn) {
        stopSpeech();
        return;
    }

    const bool current = m_speaking == m_tts
    && (m_tts->state() == TtsEngine::State::Ready
    || m_tts->state() == TtsEngine::State::Starting);

    if (!current)
        startSpeech();

    QTimer::singleShot(0, this, &MainWindow::maybeSuggestLanguagePreset);
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

// ===============================================================
// ocr and hook load
// ===============================================================

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

    syncHookControllerTargets();

    m_hookController->apply(ui->textProcessingHookCheckBox->isChecked(),
                            ui->textProcessingHookCheckBox->isEnabled());
}

void MainWindow::syncHookControllerTargets()
{
    m_hookController->setMode(m_hookMode);
    m_hookController->setCurrentGameAppPlugin(m_currentGameAppPlugin);
    m_hookController->setCurrentEnginePlugin(m_currentEnginePlugin);
    m_hookController->setCurrentEngineProcess(m_currentEngineProcess);
    m_outputWindow->setHookTarget(currentHookTargetKey());
    activateProfileForHook();

    syncPluginConfigs();
}

// ===============================================================
// hook state
// ===============================================================

QString MainWindow::currentHookTargetKey() const
{
    QString plugin, process;
    if (m_hookMode == HookSettingsDialog::HookMode::EngineMode) {
        plugin = m_currentEnginePlugin;
        process = m_currentEngineProcess;
    } else {
        plugin = m_currentGameAppPlugin;
        for (const auto &p : m_registry) {
            if (p.name == m_currentGameAppPlugin) {
                process = p.targetExecutable;
                break;
            }
        }
    }
    if (plugin.isEmpty() && process.isEmpty())
        return QString();
    return plugin + QStringLiteral("|") + process;
}

void MainWindow::clearHookState()
{
    m_hookBurstTimer->stop();
    m_hookBurstBuffer.clear();
    m_currentHookTexts.clear();
    m_outputWindow->clearResultsBySource(QStringLiteral("Hook"));
}

void MainWindow::syncPluginConfigs()
{
    const QString target = currentHookTargetKey();
    const QString plugin = target.section(QLatin1Char('|'), 0, 0);
    if (plugin.isEmpty())
        return;

    // Only per-character plugins do anything with the flush interval
    const auto it = std::find_if(m_registry.cbegin(), m_registry.cend(),
                                 [&](const PluginManager::PluginInfo &i) { return i.name == plugin; });
    if (it == m_registry.cend() || it->textMode != QLatin1String("per_char"))
        return;

    m_hookController->setPluginConfig(plugin, buildPluginConfigJson(storedFlushMs(target)));
}

QString MainWindow::buildPluginConfigJson(int flushMs) const
{
    QJsonObject root;
    root[QStringLiteral("flush_interval_ms")] = flushMs;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void MainWindow::pushCurrentPluginConfig()
{
    const QString plugin = m_hookController->currentRunningPlugin();
    if (plugin.isEmpty())
        return;
    m_hookController->setPluginConfig(plugin, buildPluginConfigJson(storedFlushMs(currentHookTargetKey())));
}

// ===============================================================
// speed settings [hook]
// ===============================================================

void MainWindow::openSpeedSettings()
{
    const QString pluginName = m_hookController->currentRunningPlugin();
    if (pluginName.isEmpty()) return;

    auto it = std::find_if(m_registry.cbegin(), m_registry.cend(),
                           [&](const PluginManager::PluginInfo &i) { return i.name == pluginName; });
    if (it == m_registry.cend()) return;

    const QString target = currentHookTargetKey();
    if (target.isEmpty()) return;

    const int chosen = m_outputWindow->promptSpeed(storedFlushMs(target));
    if (chosen < 0) return; // cancelled

    QJsonObject all = Config::getValue("plugin_flush_ms").toJsonObject();
    all[target] = chosen;
    Config::setValue("plugin_flush_ms", all);
    Config::save();

    m_hookController->setPluginConfig(pluginName, buildPluginConfigJson(chosen));
}

void MainWindow::updateSpeedButtonAvailability()
{
    const QString pluginName = m_hookController->currentRunningPlugin();
    bool eligible = false;
    if (!pluginName.isEmpty()) {
        auto it = std::find_if(m_registry.cbegin(), m_registry.cend(),
                               [&](const PluginManager::PluginInfo &i) { return i.name == pluginName; });
        eligible = it != m_registry.cend() && it->textMode == QLatin1String("per_char");
    }
    m_outputWindow->setSpeedAvailable(eligible);
}

int MainWindow::storedFlushMs(const QString &targetKey) const
{
    const QJsonObject all = Config::getValue("plugin_flush_ms").toJsonObject();
    return all.value(targetKey).toInt(TextOutputWindow::kDefaultFlushMs);
}

// ===============================================================
// config save
// ===============================================================

void MainWindow::reapplyProfileSections()
{
    setPropertyChanged(true);

    loadOutputSettings(Config::getValue("output").toJsonObject());
    loadTranslatorSettings(Config::getValue("translator").toJsonObject());
    loadTextProcessingSettings(Config::getValue("text_processing").toJsonObject());
    loadScreencastSettings();
    loadOcrSettings();
    loadSpeechSettings(Config::getValue("speech").toJsonObject());

    m_outputWindow->loadConfig();

    syncHookControllerTargets();
    m_hookController->retarget(ui->textProcessingHookCheckBox->isChecked());

    setPropertyChanged(false);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void MainWindow::refreshConfigsPage()
{
    const QString active = Config::activeProfile();
    ui->configsActiveLabel->setText(tr("Active profile: %1").arg(active));

    ui->configsListWidget->clear();
    const QStringList profiles = Config::availableProfiles();
    for (const QString &name : profiles) {
        auto *item = new QListWidgetItem(name == active ? tr("%1 (active)").arg(name) : name, ui->configsListWidget);
        item->setData(Qt::UserRole, name);
    }

    QListWidgetItem *sel = ui->configsListWidget->currentItem();
    const QString selName = sel ? sel->data(Qt::UserRole).toString() : QString();
    const bool hasSel = sel != nullptr;
    ui->configsLoadButton->setEnabled(hasSel && selName != active);
    ui->configsRenameButton->setEnabled(hasSel);
    ui->configsDeleteButton->setEnabled(hasSel && selName != active);
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
        if (widgetChanged(ui->generalHotkeySpeakTextEdit))
            general["hotkey_speak_text"] = ui->generalHotkeySpeakTextEdit->keySequence().toString();
        if (widgetChanged(ui->generalHotkeyStopSpeechEdit))
            general["hotkey_stop_speech"] = ui->generalHotkeyStopSpeechEdit->keySequence().toString();
            general["hotkey_toggle_speech"] = ui->generalHotkeyToggleSpeechEdit->keySequence().toString();
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
            msgBox.setWindowFlag(Qt::WindowStaysOnTopHint, true);
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
            commitReplacementTable();
            m_liveProfile = m_activeProfile;

            QJsonArray jsonArray;
            for (const ReplacementRule &rule : std::as_const(m_replacementRules)) {
                QJsonObject rowObject;
                rowObject["regex"] = rule.regex;
                rowObject["source"] = rule.source;
                rowObject["from"] = rule.from;
                rowObject["to"] = rule.to;
                if (!rule.profile.isEmpty())
                    rowObject["profile"] = rule.profile;
                jsonArray.append(rowObject);
            }
            textProcessing["text_replacement_table"] = jsonArray;

            QJsonArray profiles;
            for (const QString &name : std::as_const(m_ruleProfiles))
                profiles.append(name);
            textProcessing["profiles"] = profiles;
            textProcessing["active_profile"] = m_liveProfile;

            QJsonObject bindings;
            for (auto it = m_profileBindings.constBegin(); it != m_profileBindings.constEnd(); ++it) {
                if (it.value().isEmpty())
                    continue;
                QStringList targets = it.value().values();
                targets.sort();
                QJsonArray arr;
                for (const QString &target : std::as_const(targets))
                    arr.append(target);
                bindings[it.key()] = arr;
            }
            textProcessing["profile_bindings"] = bindings;
        }
        Config::setValue("text_processing", textProcessing);
        activateProfileForHook();
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

    // Python
    if (m_pythonChanged) {
        QJsonObject python = Config::getValue("python").toJsonObject();
        if (widgetChanged(ui->pythonInterpreterEdit))
            python["interpreter"] = ui->pythonInterpreterEdit->text().trimmed();
        Config::setValue("python", python);
    }

    // Speech
    if (m_speechChanged) {
        QJsonObject speech = Config::getValue("speech").toJsonObject();
        if (widgetChanged(ui->speechEnabled))
            speech["is_speech"] = ui->speechEnabled->isChecked();

        if (widgetChanged(ui->speechToggleSoundCheckBox))
            speech["toggle_sound"] = ui->speechToggleSoundCheckBox->isChecked();

        if (widgetChanged(ui->speechOfferPresetsCheckBox))
            speech["offer_tts_presets"] = ui->speechOfferPresetsCheckBox->isChecked();

        if (widgetChanged(ui->speechVolumeSlider))
            speech["volume"] = ui->speechVolumeSlider->value();

        if (widgetChanged(ui->speechTextCombo))
            speech["original"] = ui->speechTextCombo->currentData().toBool();

        if (widgetChanged(ui->speechSourceCombo))
            speech["source"] = ui->speechSourceCombo->currentData().toString();

        if (widgetChanged(ui->speechTranslatorCombo))
            speech["translator"] = ui->speechTranslatorCombo->currentData().toString();

        if (widgetChanged(ui->speechModeCombo))
            speech["manual"] = ui->speechModeCombo->currentData().toBool();

        if (widgetChanged(ui->speechOnNewTranslationCombo))
            speech["interrupt"] = ui->speechOnNewTranslationCombo->currentData().toBool();

        speech["engine"] = m_tts->id();

        for (TtsEngine *engine : m_ttsEngines) {
            QJsonObject settings = engine->saveSettings();

            if (engine == m_tts) {
                const QString voice = ui->speechVoiceCombo->currentData().toString();
                if (!voice.isEmpty())
                    settings["voice"] = voice;

                settings["speed"] = int(qRound(ui->speechSpeedSpin->value() * 100.0));
            }
            speech[engine->id()] = settings;
        }
        Config::setValue("speech", speech);
    }

    // Save Config
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    Config::save();
}
