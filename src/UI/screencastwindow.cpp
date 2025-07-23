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

#include "screencastwindow.h"
#include "ui_screencastwindow.h"
#include "src/utils/config.h"

#include <QJsonObject>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QMessageBox>

ScreenCastWindow::ScreenCastWindow(ScreenCast* screenCapture, QWidget *parent)
    : QWidget(parent)
    , m_screenCapture(screenCapture)
    , ui(new Ui::ScreenCastWindow)
{
    ui->setupUi(this);

    connect(ui->listComboBox, &QComboBox::currentIndexChanged, this, &ScreenCastWindow::onWidgetChanged);
    connect(ui->desktopRadio, &QRadioButton::toggled, this, &ScreenCastWindow::onWidgetChanged);
}

ScreenCastWindow::~ScreenCastWindow()
{
    delete ui;
}

void ScreenCastWindow::setCurrentOriginalFrame(const QImage &frame)
{
    if (!frame.isNull()) {
        ui->preview->setPixmap(QPixmap::fromImage(frame).scaled(ui->preview->size(), Qt::KeepAspectRatio));
    }
}

void ScreenCastWindow::on_desktopRadio_toggled(bool checked)
{
    m_screenCapture->setIsCaptureDesktop(checked);
    updateList();
}

void ScreenCastWindow::on_listComboBox_currentIndexChanged(int index)
{
    m_screenCapture->setIsProcessed(true);
    m_screenCapture->wakeWaitCondition();

    if (m_screenCapture->isCaptureDesktop()) {
        m_screenCapture->setCurrentDisplayIndex(index);
    } else {
        if (index < m_currentWindowsList.size() && index >= 0) {
            m_screenCapture->setCurrentWindow(m_currentWindowsList[index].window_id);
        }
    }
}

void ScreenCastWindow::on_pushButton_clicked()
{
    updateList();
}

void ScreenCastWindow::on_buttonBox_clicked(QAbstractButton *button)
{
    QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);

    if (role == QDialogButtonBox::ApplyRole) {
        saveConfig();
        setCurrentApplyButtonState(false);
    }

    hide();
}

void ScreenCastWindow::showEvent(QShowEvent *event)
{
    if (!m_screenCapture->isCaptureDesktop()) {
        ui->windowRadio->setChecked(true);
    }

    updateList();
    loadConfig();

    emit on_screencastWindowShown();
}

void ScreenCastWindow::hideEvent(QHideEvent *event)
{
    emit on_screencastWindowHidden();
}

void ScreenCastWindow::closeEvent(QCloseEvent *event)
{
    if (ui->buttonBox->button(QDialogButtonBox::Apply)->isEnabled()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Settings changed"));
        msgBox.setText(tr("There are unsaved changes. Do you want to save them?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        int ret = msgBox.exec();
        switch (ret) {
        case QMessageBox::Yes:
            event->accept();
            saveConfig();
            setCurrentApplyButtonState(false);
            break;
        case QMessageBox::No:
            event->accept();
            setCurrentApplyButtonState(false);
            break;
        case QMessageBox::Cancel:
            event->ignore();
            break;
        default:
            break;
        }
    }
}

void ScreenCastWindow::setCurrentApplyButtonState(bool value)
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(value);
}

void ScreenCastWindow::onWidgetChanged()
{
    setCurrentApplyButtonState(true);
}

void ScreenCastWindow::updateList()
{
    m_currentDisplaysList.clear();
    m_currentWindowsList.clear();
    ui->listComboBox->clear();

    if (m_screenCapture->isCaptureDesktop()) {
        updateDisplayList();
    } else {
        updateWindowList();
    }
}

void ScreenCastWindow::updateDisplayList()
{
    m_currentDisplaysList = m_screenCapture->getDisplays();
    for (const DisplayInfo& display : m_currentDisplaysList) {
        ui->listComboBox->addItem(QString("%1: %2x%3").arg(display.name).arg(display.width).arg(display.height));
    }
}

void ScreenCastWindow::updateWindowList()
{
    m_currentWindowsList = m_screenCapture->getWindows();
    for (const WindowInfo& window : m_currentWindowsList) {
#ifdef Q_OS_LINUX
        ui->listComboBox->addItem(QString("%1: %2").arg(window.window_id).arg(window.title));
#elif defined(Q_OS_WIN)
        ui->listComboBox->addItem(QString("%1: %2").arg((quint64)window.window_id).arg(window.title));
#endif
    }
}

void ScreenCastWindow::loadConfig()
{
    QJsonObject screencast = Config::getValue("screencast").toJsonObject();

    if (screencast.isEmpty()) return;

    if (m_screenCapture->isCaptureDesktop()) {
        int currentIndex = (screencast["display_index"].toInt());
        if (currentIndex != -1 && currentIndex <= m_currentDisplaysList.size() - 1)
        {
            ui->listComboBox->setCurrentIndex(currentIndex);
            setCurrentApplyButtonState(false);
        }
    } else {
#ifdef Q_OS_LINUX
        unsigned long currentWindow = (screencast["window_id"].toInt());
#elif defined(Q_OS_WIN)
        uintptr_t currentWindow = screencast["window_id"].toVariant().toULongLong();
#endif
        for (int i = 0; i < ui->listComboBox->count(); ++i) {
            QString text = ui->listComboBox->itemText(i);
            QStringList parts = text.split(":");
            if (parts.count() > 0 && parts[0].toInt() == currentWindow) {
                ui->listComboBox->setCurrentIndex(i);
                setCurrentApplyButtonState(false);
                break;
            }
        }
    }
}

void ScreenCastWindow::saveConfig()
{
    bool isCaptureDesktop = ui->desktopRadio->isChecked();
    int currentIndex = ui->listComboBox->currentIndex();

    QJsonObject screencast;

    if (isCaptureDesktop) {
        screencast["display_index"]                   = ui->listComboBox->currentIndex();
    } else {
        screencast["window_id"]                       = QJsonValue::fromVariant(QVariant(qlonglong(m_currentWindowsList[currentIndex].window_id)));
    }
    screencast["is_capture_desktop"]                  = isCaptureDesktop;

    Config::setValue("screencast", screencast);
    Config::saveConfig("settings.json");
}
