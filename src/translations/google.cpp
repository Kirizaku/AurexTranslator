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

void Google::translateText(QString text, std::function<void(TranslationResult)> callback)
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

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool failed = (reply->error() != QNetworkReply::NoError);
        const QString error = reply->errorString();
        const QJsonArray jsonArray = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();

        if (status == 429) {
            callback({TranslationResult::RateLimited, QString()});
            return;
        }

        if (failed) {
            callback({TranslationResult::Error, error});
            return;
        }

        QString translatedText;
        for (const QJsonValue &val : jsonArray.first().toArray()) {
            if (val.isArray() && !val.toArray().isEmpty()) {
                translatedText += val.toArray()[0].toString();
            }
        }

        if (translatedText.isEmpty()) {
            callback({TranslationResult::Error, tr("no translation in the response")});
            return;
        }
        callback({TranslationResult::Ok, translatedText});
    });
}
