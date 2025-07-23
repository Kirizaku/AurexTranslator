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

#include "textoutputwindow.h"
#include "ui_textoutputwindow.h"

#include <QMenu>
#include <QWindow>
#include <QClipboard>
#include <QWidgetAction>
#include <QProcessEnvironment>
#include <QJsonObject>
#include <QMessageBox>
#include <QScreen>
#include <QMouseEvent>
#include <QTimer>
#include <QScrollBar>
#include <QColorDialog>

#include "src/utils/config.h"

TextOutputWindow::TextOutputWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TextOutputWindow)
    , m_historyTextEdit(new QPlainTextEdit)
{
    ui->setupUi(this);
    ui->toolBar->hide();

    m_historyTextEdit->setWindowTitle(tr("Translation history"));
    m_historyTextEdit->setReadOnly(true);
    m_historyTextEdit->resize(640, 360);

    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::WindowStaysOnTopHint |
                   Qt::FramelessWindowHint
#ifdef Q_OS_LINUX
                   | Qt::X11BypassWindowManagerHint
#endif
                   );

    createMenus();
    loadConfig();

    ui->label->setText(m_initText);

    hideTimer = new QTimer(this);
    hideTimer->setSingleShot(true);
    connect(hideTimer, &QTimer::timeout, ui->toolBar, &QWidget::hide);
    connect(ui->historyButton, &QPushButton::clicked, this, &TextOutputWindow::showHistory);
}

TextOutputWindow::~TextOutputWindow()
{
    saveConfig();
    delete ui;
}

void TextOutputWindow::setCurrentOutputOCR(const QString &translatorName, const QString &original, const QString &result)
{
    m_translatorsResults[translatorName] = result;
    m_originalTexts[translatorName] = original;
    int scrollValue = m_historyTextEdit->verticalScrollBar()->value();
    QString historyItem = QString(tr("Translator: %1\nOriginal:\n %2\nResult:\n %3"))
                              .arg(translatorName)
                              .arg(original)
                              .arg(result);
    m_translationHistory.append(historyItem);

    QString historyText;
    for (const auto &item : m_translationHistory) {
        historyText += item + "\n";
    }
    m_historyTextEdit->setPlainText(historyText);
    m_historyTextEdit->verticalScrollBar()->setValue(scrollValue);

    updateText();
}

void TextOutputWindow::clearOverlayText(const QString &translatorName)
{
    if (m_translatorsResults.contains(translatorName)) {
        m_translatorsResults.remove(translatorName);
    }
    if (m_originalTexts.contains(translatorName)) {
        m_originalTexts.remove(translatorName);
    }
    updateText();
}

void TextOutputWindow::showHistory()
{
    m_historyTextEdit->show();
}


bool TextOutputWindow::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::Enter: {
        QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
        updateCursorShape(hoverEvent->position().toPoint());
        ui->toolBar->show();
        hideTimer->stop();
        return true;
    }

    case QEvent::Leave: {
        hideTimer->start(1000);
        return true;
    }

    case QEvent::MouseMove: {
        QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
        if (m_isResizing)
        {
            QRect newGeometry = m_dragStartGeometry;
            QPoint delta = hoverEvent->globalPosition().toPoint() - m_dragStartPosition;

            if (m_resizeLeft) {
                newGeometry.setLeft(newGeometry.left() + delta.x());
                if (newGeometry.width() < minimumWidth()) {
                    newGeometry.setLeft(newGeometry.right() - minimumWidth());
                }
            }
            else if (m_resizeRight) {
                newGeometry.setRight(newGeometry.right() + delta.x());
                if (newGeometry.width() < minimumWidth()) {
                    newGeometry.setRight(newGeometry.left() + minimumWidth());
                }
            }

            if (m_resizeTop) {
                newGeometry.setTop(newGeometry.top() + delta.y());
                if (newGeometry.height() < minimumHeight()) {
                    newGeometry.setTop(newGeometry.bottom() - minimumHeight());
                }
            }
            else if (m_resizeBottom) {
                newGeometry.setBottom(newGeometry.bottom() + delta.y());
                if (newGeometry.height() < minimumHeight()) {
                    newGeometry.setBottom(newGeometry.top() + minimumHeight());
                }
            }
            setGeometry(newGeometry);
        } else if (m_isMoving) {
            QPoint newPos = hoverEvent->globalPosition().toPoint() - m_dragPosition;
            move(newPos);
        } else {
            updateCursorShape(hoverEvent->position().toPoint());
        }
        return true;
    }
    default:
        return QWidget::event(event);
    }
}

void TextOutputWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int margin = 10;
        QRect rect = this->rect();
        QPoint pos = event->pos();

        m_resizeLeft = m_resizeRight = m_resizeTop = m_resizeBottom = false;

        if (pos.x() <= margin) m_resizeLeft = true;
        if (pos.x() >= rect.width() - margin) m_resizeRight = true;
        if (pos.y() <= margin) m_resizeTop = true;
        if (pos.y() >= rect.height() - margin) m_resizeBottom = true;

        if (m_resizeLeft || m_resizeRight || m_resizeTop || m_resizeBottom) {
            m_isResizing = true;
            m_dragStartPosition = event->globalPosition().toPoint();
            m_dragStartGeometry = geometry();
            updateCursorShape(pos);
            return;
        }
        m_isMoving = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);

        activateWindow();
    }
}

void TextOutputWindow::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_isResizing = false;
    m_isMoving = false;
    setCursor(Qt::ArrowCursor);
}

void TextOutputWindow::closeEvent(QCloseEvent* event)
{
    QApplication::quit();
}

void TextOutputWindow::on_selectionZoneButton_clicked()
{
    emit on_selectNewRegion();
}

void TextOutputWindow::on_createIgnoreZoneButton_clicked()
{
    emit on_selectNewInnerRegion();
}

void TextOutputWindow::on_copyButton_clicked()
{
    QApplication::clipboard()->setText(ui->label->text());
}

void TextOutputWindow::on_retranslateButton_clicked()
{
    emit on_retranslate();
}

void TextOutputWindow::on_settingsButton_clicked()
{
    m_contextMenu->exec(ui->toolBar->mapToGlobal(QPoint(ui->toolBar->width(), 0)));
}

void TextOutputWindow::on_exitButton_clicked()
{
    close();
}

void TextOutputWindow::updateMargin()
{
    int top = m_marginTop->value();
    int bottom = m_marginBottom->value();
    int left = m_marginLeft->value();
    int right = m_marginRight->value();

    ui->content->setContentsMargins(left, top, right, bottom);
}

void TextOutputWindow::updateText()
{
    if (!m_translatorsResults.isEmpty())
    {
        QString text;
        for (auto it = m_translatorsResults.begin(); it != m_translatorsResults.end(); ++it) {
            if (m_showTranslatorName->isChecked()) {
                text += it.key() + "\n";
            }
            if (m_showOriginalText->isChecked()) {
                text += m_originalTexts[it.key()] + "\n";
            }
            text += it.value() + "\n";
        }
        ui->label->setText(text);
    } else {
        ui->label->clear();
        ui->label->setText(m_initText);
    }
}

void TextOutputWindow::updateCursorShape(const QPoint& pos)
{
    const int margin = 10;
    QRect rect = this->rect();
    bool left = pos.x() <= margin;
    bool right = pos.x() >= rect.width() - margin;
    bool top = pos.y() <= margin;
    bool bottom = pos.y() >= rect.height() - margin;

    if ((left && top) || (right && bottom)) {
        setCursor(Qt::SizeFDiagCursor);
    }
    else if ((right && top) || (left && bottom)) {
        setCursor(Qt::SizeBDiagCursor);
    }
    else if (left || right) {
        setCursor(Qt::SizeHorCursor);
    }
    else if (top || bottom) {
        setCursor(Qt::SizeVerCursor);
    }
    else {
        setCursor(Qt::ArrowCursor);
    }
}

void TextOutputWindow::createMenus()
{
    m_contextMenu = new QMenu(this);

    QWidget *widget = new QWidget();

    m_fontComboBox = new QFontComboBox();
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 24);
    m_textColorButton = new QPushButton();
    m_textColorButton->setStyleSheet("background-color: white");

    m_alignmentComboBox = new QComboBox();
    m_alignmentComboBox->addItem(tr("Left"), Qt::AlignLeft);
    m_alignmentComboBox->addItem(tr("Right"), Qt::AlignRight);
    m_alignmentComboBox->addItem(tr("Center"), Qt::AlignCenter);

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 255);

    QWidgetAction *action = new QWidgetAction(m_contextMenu);
    action->setDefaultWidget(widget);
    m_contextMenu->addAction(action);

    m_marginTop = new QSpinBox;
    m_marginBottom = new QSpinBox;
    m_marginLeft = new QSpinBox;
    m_marginRight = new QSpinBox;

    m_showOriginalText = new QCheckBox();
    m_showOriginalText->setText(tr("Show/Hide original text"));

    m_showTranslatorName = new QCheckBox();
    m_showTranslatorName->setText(tr("Show/Hide translator name"));

    QFormLayout *formLayout = new QFormLayout(widget);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->addRow(tr("Transparency"), m_opacitySlider);
    formLayout->addRow(tr("Font"), m_fontComboBox);
    formLayout->addRow(tr("Font size"), m_fontSizeSpinBox);
    formLayout->addRow(tr("Text color"), m_textColorButton);
    formLayout->addRow(tr("Text alignment"), m_alignmentComboBox);
    formLayout->addRow(tr("Margin top"), m_marginTop);
    formLayout->addRow(tr("Margin bottom"), m_marginBottom);
    formLayout->addRow(tr("Margin left"), m_marginLeft);
    formLayout->addRow(tr("Margin right"), m_marginRight);
    formLayout->addRow(m_showOriginalText);
    formLayout->addRow(m_showTranslatorName);

    connect(m_fontComboBox, &QFontComboBox::currentFontChanged, [this](const QFont &font) {
        m_fontSizeSpinBox->setValue(font.pointSize());
        ui->label->setFont(font);
    });

    connect(m_fontSizeSpinBox, &QSpinBox::valueChanged, [this](int value) {
        QFont font = m_fontComboBox->currentFont();
        font.setPointSize(value);
        ui->label->setFont(font);
    });

    m_fontSizeSpinBox->setValue(12);

    connect(m_textColorButton, &QPushButton::clicked, [=] {
        QColorDialog colorDialog(this);
        colorDialog.setCurrentColor(m_currentTextColor);
        colorDialog.move(ui->toolBar->mapToGlobal(QPoint(ui->toolBar->width(), 0)));
        colorDialog.exec();
        QColor color = colorDialog.currentColor();
        if (color.isValid()) {
            ui->label->setStyleSheet("color: " + color.name());
            m_currentTextColor = color;
            m_textColorButton->setStyleSheet("background-color: " + color.name() + "; color: " + (color.lightness() > 128 ? "black" : "white"));
        }
    });

    connect(m_alignmentComboBox, &QComboBox::currentIndexChanged, [this](int index) {
        Qt::Alignment alignment = static_cast<Qt::Alignment>(m_alignmentComboBox->itemData(index).toInt());
        ui->label->setAlignment(alignment);
    });

    connect(m_marginTop, &QSpinBox::valueChanged, this, &TextOutputWindow::updateMargin);
    connect(m_marginBottom, &QSpinBox::valueChanged, this, &TextOutputWindow::updateMargin);
    connect(m_marginLeft, &QSpinBox::valueChanged, this, &TextOutputWindow::updateMargin);
    connect(m_marginRight, &QSpinBox::valueChanged, this, &TextOutputWindow::updateMargin);

    m_marginTop->setValue(9);
    m_marginBottom->setValue(9);
    m_marginLeft->setValue(9);
    m_marginRight->setValue(9);

    connect(m_opacitySlider, &QSlider::valueChanged, [this](int value) {
        ui->content->setStyleSheet("#content { background-color: rgba(20, 20, 20, " + QString::number(value) + "); }");
    });

    connect(m_showOriginalText, &QCheckBox::stateChanged, this, &TextOutputWindow::updateText);
    connect(m_showTranslatorName, &QCheckBox::stateChanged, this, &TextOutputWindow::updateText);
}

bool TextOutputWindow::isOutOfBounds(const QJsonObject& output_window)
{
    int x = output_window["x"].toInt();
    int y = output_window["y"].toInt();
    int width = output_window["width"].toInt();
    int height = output_window["height"].toInt();

    QList<QRect> screens;
    foreach (QScreen *screen, QGuiApplication::screens()) {
        screens.append(screen->geometry());
    }

    QRect rect(x, y, width, height);
    for (const QRect& screen : screens) {
        if (screen.intersects(rect)) {
            return false;
        }
    }
    return true;
}

void TextOutputWindow::loadConfig()
{
    QJsonObject output_window = Config::getValue("output_window").toJsonObject();

    if (output_window.isEmpty()) {
        m_opacitySlider->setValue(255);
        QScreen *screen = QApplication::primaryScreen();
        move(screen->geometry().x() + 50, screen->geometry().y() + 50);
        return;
    }

    if (!isOutOfBounds(output_window)) {
        setGeometry(output_window["x"].toInt(), output_window["y"].toInt(), output_window["width"].toInt(), output_window["height"].toInt());
    }

    m_opacitySlider->setValue(output_window["opacity"].toInt());

    QFont font = ui->label->font();
    font.setFamily(output_window["font"].toString());
    m_fontComboBox->setCurrentText(output_window["font"].toString());
    ui->label->setFont(font);
    m_currentTextColor.setNamedColor(output_window["text_color"].toString());
    ui->label->setStyleSheet("color: " + m_currentTextColor.name());
    m_textColorButton->setStyleSheet("background-color: " + m_currentTextColor.name() + "; color: " + (m_currentTextColor.lightness() > 128 ? "black" : "white"));
    m_fontSizeSpinBox->setValue(output_window["font_size"].toInt());
    m_alignmentComboBox->setCurrentIndex(output_window["text_alignment"].toInt());
    m_marginTop->setValue(output_window["margin_top"].toInt());
    m_marginBottom->setValue(output_window["margin_bottom"].toInt());
    m_marginLeft->setValue(output_window["margin_left"].toInt());
    m_marginRight->setValue(output_window["margin_right"].toInt());
    m_showOriginalText->setChecked(output_window["is_show_original_text"].toBool());
    m_showTranslatorName->setChecked(output_window["is_show_translator_name"].toBool());
}

void TextOutputWindow::saveConfig()
{
    QJsonObject output_window;
    output_window["x"] = x();
    output_window["y"] = y();
    output_window["width"] = width();
    output_window["height"] = height();
    output_window["opacity"] = m_opacitySlider->value();
    QFont font = ui->label->font();
    output_window["font"] = m_fontComboBox->currentText();
    output_window["font_size"] = font.pointSize();
    output_window["text_color"] = m_currentTextColor.name();
    output_window["text_alignment"] = m_alignmentComboBox->currentIndex();
    output_window["margin_top"] = m_marginTop->value();
    output_window["margin_bottom"] = m_marginBottom->value();
    output_window["margin_left"] = m_marginLeft->value();
    output_window["margin_right"] = m_marginRight->value();
    output_window["is_show_original_text"] = m_showOriginalText->isChecked();
    output_window["is_show_translator_name"] = m_showTranslatorName->isChecked();

    Config::setValue("output_window", output_window);
    Config::saveConfig("settings.json");
}

