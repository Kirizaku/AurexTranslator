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

#ifndef TESSERACTSETTINGSDIALOG_H
#define TESSERACTSETTINGSDIALOG_H

#include <QDialog>

#include "../utils/tesseractocr.h"

class QLabel;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QRadioButton;
class QSpinBox;
class QButtonGroup;
class QPlainTextEdit;

class TesseractSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TesseractSettingsDialog(const QString &status,
                                    const QString &currentLanguage,
                                    const QStringList &LanguageList,
                                    const QString &tessdataPath,
                                    bool useSystemTessdata,
                                    int mode,
                                    int autoInterval,
                                    TesseractOcr *m_tesseractOcr,
                                    QWidget *parent = nullptr);
    ~TesseractSettingsDialog();

    QString getCurrentLanguage() const;
    QStringList getLanguageList() const;
    QString getTessdataPath() const;
    bool getUseSystemTessdata() const;
    int getMode() const;
    int getAutoInterval() const;

private slots:
    void on_updateLanguagesButton_clicked();

private:
    TesseractOcr *m_tesseractOcr;
    QLabel *m_statusLabel;
    QComboBox *m_comboBox;
    QLineEdit *m_pathLineEdit;
    QCheckBox *m_systemTessdataCheckBox;
    QRadioButton *m_autoRadio;
    QRadioButton *m_manualRadio;
    QSpinBox *m_intervalSpinBox;
    QButtonGroup *m_modeGroup;

    enum Mode {
        Auto,
        Manual
    };
};

#endif // TESSERACTSETTINGSDIALOG_H
