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

#ifndef TEXTOUTPUTWINDOW_H
#define TEXTOUTPUTWINDOW_H

#include <QWidget>
#include <QSlider>
#include <QFontComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>

namespace Ui {
class TextOutputWindow;
}

class TextOutputWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TextOutputWindow(QWidget *parent = nullptr);
    ~TextOutputWindow();

signals:
    void on_retranslate();
    void on_selectNewRegion();
    void on_selectNewInnerRegion();

public slots:
    void setCurrentOutputOCR(const QString &translatorName, const QString &original, const QString &result);
    void clearOverlayText(const QString &translatorName);
    void showHistory();

protected:
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_selectionZoneButton_clicked();
    void on_createIgnoreZoneButton_clicked();
    void on_copyButton_clicked();
    void on_retranslateButton_clicked();
    void on_settingsButton_clicked();
    void on_exitButton_clicked();
    void updateMargin();
    void updateText();

private:
    Ui::TextOutputWindow *ui;
    QString m_initText = tr("Welcome!");
    QColor m_currentTextColor = Qt::white;
    QTimer *hideTimer;

    void updateCursorShape(const QPoint& pos);

    QMenu *m_contextMenu;
    QSlider* m_opacitySlider;
    QFontComboBox *m_fontComboBox;
    QSpinBox *m_fontSizeSpinBox;
    QPushButton *m_textColorButton;
    QComboBox *m_alignmentComboBox;
    QSpinBox *m_marginTop;
    QSpinBox *m_marginBottom;
    QSpinBox *m_marginLeft;
    QSpinBox *m_marginRight;
    QCheckBox *m_showOriginalText;
    QCheckBox* m_showTranslatorName;
    QPlainTextEdit *m_historyTextEdit;
    void createMenus();

    bool m_resizeLeft = false;
    bool m_resizeRight = false;
    bool m_resizeTop = false;
    bool m_resizeBottom = false;
    bool m_isResizing = false;
    bool m_isMoving = false;
    QPoint m_dragPosition;
    QRect m_dragStartGeometry;
    QPoint m_dragStartPosition;

    bool isOutOfBounds(const QJsonObject& overlay);
    void loadConfig();
    void saveConfig();

    QList<QString> m_translationHistory;
    QMap<QString, QString> m_translatorsResults;
    QMap<QString, QString> m_originalTexts;
};

#endif // TEXTOUTPUTWINDOW_H
