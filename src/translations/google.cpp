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

#include <QtNetwork/QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrlQuery>

#include "google.h"

Google::Google(QNetworkAccessManager *manager, QObject *parent)
    : QObject{parent}, m_manager(manager) {}

Google::~Google() {}

void Google::translateText(QString text, std::function<void(QString)> callback)
{
    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery query;
    query.addQueryItem("client", "gtx");
    query.addQueryItem("sl", sourceLang);
    query.addQueryItem("tl", targetLang);
    query.addQueryItem("dt", "t");
    query.addQueryItem("q", text);
    url.setQuery(query);

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");

    QNetworkReply *reply = m_manager->get(request);

    connect(reply, &QNetworkReply::finished, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            QJsonArray jsonArray = jsonDoc.array();

            if (!jsonArray.isEmpty() && jsonArray[0].isArray()) {
                QJsonArray translations = jsonArray[0].toArray();
                QString translatedText;

                for (const QJsonValue &val : translations) {
                    if (val.isArray() && !val.toArray().isEmpty()) {
                        translatedText += val.toArray()[0].toString();
                    }
                }
                callback(translatedText);
            }
        } else {
            callback("Translation failed:" + reply->errorString());
        }
        reply->deleteLater();
    });
}
