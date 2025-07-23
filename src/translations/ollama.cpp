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
#include <QUrl>

#include "ollama.h"

Ollama::Ollama(QNetworkAccessManager *manager, QObject *parent)
    : QObject{parent}, m_manager(manager) {}

Ollama::~Ollama() {}

void Ollama::checkServerAvailable(const QUrl &url, std::function<void(bool)> callback)
{
    m_url = url;
    QUrl urlTags = m_url.resolved(QUrl("api/tags"));
    QNetworkRequest request(urlTags);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, [=]() {
        bool isAvailable = (reply->error() == QNetworkReply::NoError);
        callback(isAvailable);
        reply->deleteLater();
    });
}

void Ollama::checkModelsAvailable(std::function<void(QStringList)> callback)
{
    QUrl url = m_url.resolved(QUrl("api/tags"));
    QNetworkRequest request(url);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, [=]() {
        QStringList models;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            QJsonArray modelsArray = jsonResponse.object()["models"].toArray();

            for (const auto &model : modelsArray) {
                models.append(model.toObject()["name"].toString());
            }
        }
        callback(models);
        reply->deleteLater();
    });
}

void Ollama::generate(const QString &prompt, const QString &model, std::function<void(QString)> callback)
{    
    QUrl url = m_url.resolved(QUrl("api/generate"));
    QJsonObject json;
    json["model"] = model;
    json["prompt"] = prompt;
    json["stream"] = false;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_manager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            QString result = jsonResponse.object()["response"].toString();
            callback(result);
        } else {
            callback(reply->errorString());
        }
        reply->deleteLater();
    });
}

// void Ollama::generateVision(const QString &prompt, const QString &model, std::function<void(QString)> callback)
// {
//     if (!m_imageOcr.empty()) {
//         std::vector<uchar> buffer;
//         cv::imencode(".png", m_imageOcr, buffer);

//         QByteArray imageBytes(reinterpret_cast<char*>(buffer.data()), buffer.size());
//         QString base64Image = QString::fromLatin1(imageBytes.toBase64());

//         QUrl url = m_url.resolved(QUrl("api/generate"));
//         QJsonObject json;
//         json["model"] = model;
//         json["prompt"] = prompt;
//         json["images"] = QJsonArray{base64Image};
//         json["stream"] = false;

//         QNetworkRequest request(url);
//         request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

//         QNetworkReply *reply = m_manager->post(request, QJsonDocument(json).toJson());
//         connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
//             if (reply->error() == QNetworkReply::NoError) {
//                 QByteArray response = reply->readAll();
//                 QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
//                 QString result = jsonResponse.object()["response"].toString();
//                 callback(result);
//             } else {
//                 callback(reply->errorString());
//             }
//             reply->deleteLater();
//         });
//     }
// }
