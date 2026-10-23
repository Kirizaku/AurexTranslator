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

#include "../engines/tesseractocr.h"

namespace Ui {
class TesseractSettingsDialog;
}

struct TesseractConfig
{
    QString language;
    QStringList languages;
    QString tessdataPath = QStringLiteral("./tessdata");
    bool useSystemTessdata = false;
    int mode = 0;
    int autoInterval = 1;
    TesseractTuning tuning;

    bool sameSettings(const TesseractConfig &other) const
    {
        return language == other.language && tessdataPath == other.tessdataPath
            && useSystemTessdata == other.useSystemTessdata && mode == other.mode
            && autoInterval == other.autoInterval && tuning == other.tuning;
    }
};

class TesseractSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TesseractSettingsDialog(const QString &status,
                                    const TesseractConfig &config,
                                    TesseractOcr *m_tesseractOcr,
                                    QWidget *parent = nullptr);
    ~TesseractSettingsDialog();

    void setValues(const QString &status, const TesseractConfig &config);
    TesseractConfig getConfig() const;

signals:
    void applyRequested();

private slots:
    void on_updateLanguagesButton_clicked();

private:
    TesseractTuning currentTuning() const;
    void updateValueLabels();
    void updateEnabledState();
    void updateApplyButton();

    Ui::TesseractSettingsDialog *const ui;
    TesseractOcr *m_tesseractOcr;
    TesseractConfig m_originalConfig;
    bool m_loading = false;

    enum Mode {
        Auto,
        Manual
    };
};

#endif // TESSERACTSETTINGSDIALOG_H
