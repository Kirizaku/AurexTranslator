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

#include "pipersettingsdialog.h"

#include "src/engines/pipertts.h"
#include "src/engines/pipervoicecatalog.h"
#include "src/utils/dialogutils.h"

#include <QDir>
#include <QLabel>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>

PiperSettingsDialog::PiperSettingsDialog(PiperTts *piper, QWidget *parent)
    : QDialog(parent)
    , m_piper(piper)
    , m_catalog(piper->catalog())
{
    setWindowTitle(piper->name());
    setMinimumSize(650, 450);

    // Server
    m_modeManaged = new QRadioButton(tr("Install and run Piper for me"));
    m_modeExternal = new QRadioButton(tr("Use a Piper server I already run"));
    m_modeExternal->setChecked(piper->useExternalServer());
    m_modeManaged->setChecked(!piper->useExternalServer());

    m_serverUrl = new QLineEdit(piper->externalUrl());
    m_serverUrl->setPlaceholderText(QStringLiteral("http://127.0.0.1:5000"));
    m_checkButton = new QPushButton(tr("Check"));

    QHBoxLayout *serverLayout = new QHBoxLayout;
    serverLayout->addWidget(m_serverUrl);
    serverLayout->addWidget(m_checkButton);

    m_statusLabel = new QLabel;

    QFormLayout *serverForm = new QFormLayout;
    serverForm->addRow(m_modeManaged);
    serverForm->addRow(m_modeExternal);
    serverForm->addRow(tr("Server address"), serverLayout);
    serverForm->addRow(tr("Status"), m_statusLabel);

    QGroupBox *serverBox = new QGroupBox(tr("Server"));
    serverBox->setLayout(serverForm);

    // Voices to download
    m_languageCombo = new QComboBox;
    m_voiceCombo = new QComboBox;

    for (QComboBox *box : {m_languageCombo, m_voiceCombo}) {
        box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        box->setMinimumWidth(280);
    }

    m_refreshButton = new QPushButton(tr("Refresh list"));
    m_downloadButton = new QPushButton(tr("Download"));
    m_cancelButton = new QPushButton(tr("Cancel download"));

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);

    m_openFolderButton = new QPushButton(tr("Open the voices folder"));
    m_openFolderButton->setToolTip(tr("Models can also be put there by hand: <name>.onnx next to <name>.onnx.json."));

    QHBoxLayout *voiceButtons = new QHBoxLayout;
    voiceButtons->addWidget(m_refreshButton);
    voiceButtons->addWidget(m_downloadButton);
    voiceButtons->addWidget(m_openFolderButton);
    voiceButtons->addStretch();

    m_repoEdit = new QLineEdit(PiperVoiceCatalog::repoBase());
    m_repoEdit->setPlaceholderText(PiperVoiceCatalog::defaultRepoBase());
    m_repoEdit->setToolTip(tr("A prefix that file names are appended to, not a page to "
                              "open in a browser. It is expected to answer with the file "
                              "itself, the way HuggingFace does under /resolve/<branch>/."));
    m_repoResetButton = new QPushButton(tr("Default"));

    QHBoxLayout *repoLayout = new QHBoxLayout;
    repoLayout->addWidget(m_repoEdit);
    repoLayout->addWidget(m_repoResetButton);

    QFormLayout *voiceForm = new QFormLayout;
    voiceForm->addRow(tr("Repository"), repoLayout);
    voiceForm->addRow(tr("Language"), m_languageCombo);
    voiceForm->addRow(tr("Voice"), m_voiceCombo);
    voiceForm->addRow(voiceButtons);

    m_voiceBox = new QGroupBox(tr("Available voices"));
    m_voiceBox->setLayout(voiceForm);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    QHBoxLayout *downloadRow = new QHBoxLayout;
    downloadRow->addWidget(m_progress);
    downloadRow->addWidget(m_cancelButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(serverBox);
    layout->addWidget(m_voiceBox);
    layout->addLayout(downloadRow);
    layout->addWidget(buttons);

    connect(m_modeExternal, &QRadioButton::toggled, this, [this](bool external) {
        m_piper->setUseExternalServer(external);
        refreshState();
        refreshVoices();
    });
    connect(m_serverUrl, &QLineEdit::textChanged, this, [this](const QString &url) { m_piper->setExternalUrl(url); });
    connect(m_checkButton, &QPushButton::clicked, this, [this] { m_piper->start(); });
    connect(m_refreshButton, &QPushButton::clicked, this, [this] {
        m_catalog->refresh();
        m_progress->setRange(0, 0);
        refreshState();
    });
    connect(m_repoEdit, &QLineEdit::textChanged, this, [](const QString &text) { PiperVoiceCatalog::setRepoBase(text); });
    connect(m_repoResetButton, &QPushButton::clicked, this, [this] { m_repoEdit->setText(PiperVoiceCatalog::defaultRepoBase()); });
    connect(m_openFolderButton, &QPushButton::clicked, this, [this] {
        QDir dir(PiperVoiceCatalog::voicesDir());
        if (!dir.exists())
            dir.mkpath(QStringLiteral("."));

        QDesktopServices::openUrl(QUrl::fromLocalFile(dir.path()));
    });

    connect(m_downloadButton, &QPushButton::clicked, this, &PiperSettingsDialog::downloadSelected);
    connect(m_cancelButton, &QPushButton::clicked, this, [this] {
        m_cancelRequested = true;
        m_cancelButton->setEnabled(false);
        m_catalog->cancelDownload();
    });
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int) { refreshVoices(); });
    connect(m_piper, &PiperTts::stateChanged, this, [this](TtsEngine::State) { refreshState(); });
    connect(m_piper, &PiperTts::errorOccurred, this, [this](const QString &error) { m_statusLabel->setText(error); });

    connect(m_catalog, &PiperVoiceCatalog::refreshed, this, [this](bool ok, const QString &error) {
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
        refreshState();

        if (!ok) {
            DialogUtils::warning(this, m_piper->name(),
                                 tr("Could not fetch the voice list: %1\n\n"
                                    "Installed voices still work. You can change the "
                                    "repository address, or put models into the voices "
                                    "folder yourself.")
                                     .arg(error));
            return;
        }
        refreshVoices();
    });

    connect(m_catalog, &PiperVoiceCatalog::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0)
            m_progress->setValue(static_cast<int>(received * 100 / total));
    });

    connect(m_catalog, &PiperVoiceCatalog::downloadFinished, this,
            [this](const QString &key, bool ok, const QString &error) {
                const bool cancelled = m_cancelRequested;
                m_cancelRequested = false;

                m_progress->setValue(0);
                refreshState();

                if (!ok) {
                    if (!cancelled)
                        DialogUtils::warning(this, m_piper->name(), error);
                    return;
                }

                if (m_piper->voice().isEmpty())
                    m_piper->setVoice(key);

                refreshVoices();
            });

    refreshVoices();
    refreshState();
}

void PiperSettingsDialog::refreshState()
{
    const bool external = m_modeExternal->isChecked();

    m_serverUrl->setEnabled(external);
    m_checkButton->setEnabled(external);
    const bool loadingList = m_catalog->refreshing();
    m_voiceBox->setEnabled(!external && !loadingList);
    m_voiceBox->setToolTip(external ? tr("The server brings its own voices.") : QString());
    const bool downloading = m_catalog->downloading();
    m_downloadButton->setEnabled(!external && !downloading && !loadingList);
    m_refreshButton->setEnabled(!external && !downloading && !loadingList);
    m_cancelButton->setEnabled(downloading);

    QString status;
    switch (m_piper->state()) {
    case TtsEngine::State::Stopped:
        status = tr("Not running");
        break;
    case TtsEngine::State::Starting:
        status = tr("Starting...");
        break;
    case TtsEngine::State::Ready:
        status = tr("Ready at %1, voice %2").arg(m_piper->baseUrl(), m_piper->voice());
        break;
    case TtsEngine::State::Failed:
        status = tr("Failed");
        break;
    }
    m_statusLabel->setText(status);
}

void PiperSettingsDialog::refreshVoices()
{
    if (m_modeExternal->isChecked())
        return;

    const QSignalBlocker languageBlocker(m_languageCombo);
    const QSignalBlocker voiceBlocker(m_voiceCombo);

    const QList<PiperVoice> all = m_catalog->voices();
    const QStringList installed = PiperVoiceCatalog::installedVoices();

    const QString language = m_languageCombo->currentData().toString();
    const QString voice = m_voiceCombo->currentData().toString();

    m_languageCombo->clear();
    m_languageCombo->addItem(tr("Installed"), QString());

    for (const QString &code : m_catalog->languages()) {
        QString title = code;
        for (const PiperVoice &candidate : all) {
            if (candidate.languageCode == code && !candidate.languageName.isEmpty()) {
                title = QStringLiteral("%1 (%2)").arg(candidate.languageName, code);
                break;
            }
        }
        m_languageCombo->addItem(title, code);
    }

    const int languageIndex = m_languageCombo->findData(language);
    m_languageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    const QString selected = m_languageCombo->currentData().toString();

    m_voiceCombo->clear();
    if (selected.isEmpty()) {
        for (const QString &key : installed)
            m_voiceCombo->addItem(key, key);
    } else {
        for (const PiperVoice &candidate : all) {
            if (candidate.languageCode != selected)
                continue;

            const QString label =
                installed.contains(candidate.key)
                    ? tr("%1 — installed").arg(candidate.key)
                    : QStringLiteral("%1 — %2 MB")
                          .arg(candidate.key)
                          .arg(candidate.sizeBytes / 1048576.0, 0, 'f', 1);
            m_voiceCombo->addItem(label, candidate.key);
        }
    }

    const int voiceIndex = m_voiceCombo->findData(voice.isEmpty() ? m_piper->voice() : voice);
    if (voiceIndex >= 0)
        m_voiceCombo->setCurrentIndex(voiceIndex);
}

void PiperSettingsDialog::downloadSelected()
{
    const QString key = m_voiceCombo->currentData().toString();
    if (key.isEmpty())
        return;

    if (PiperVoiceCatalog::isInstalled(key)) {
        DialogUtils::information(this, m_piper->name(), tr("This voice is already installed."));
        return;
    }

    const PiperVoice voice = m_catalog->voice(key);
    const QString size = voice.sizeBytes > 0
                             ? tr("%1 MB").arg(voice.sizeBytes / 1048576.0, 0, 'f', 1)
                             : tr("unknown size");

    if (DialogUtils::question(this, tr("Download voice"),
                              tr("Download %1 (%2) from HuggingFace?").arg(key, size))
        != QMessageBox::Yes)
        return;

    m_cancelRequested = false;
    m_catalog->download(key);
    refreshState();
}
