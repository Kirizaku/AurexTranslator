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

#ifndef GOOGLE_H
#define GOOGLE_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>

class Google : public QObject
{
    Q_OBJECT
public:
    explicit Google(QNetworkAccessManager *manager, QObject *parent = nullptr);
    ~Google();
    void translateText(QString text, std::function<void(QString)> callback);

    void setSourceLang(const QString &sourceLangCode) { sourceLang = sourceLangCode; }
    void setTargetLang(const QString &targetLangCode) { targetLang = targetLangCode; }

private:
    QNetworkAccessManager *m_manager;

    QString sourceLang = "";
    QString targetLang = "";
};

#endif // GOOGLE_H
