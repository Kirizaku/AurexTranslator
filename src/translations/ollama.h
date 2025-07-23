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

#ifndef OLLAMA_H
#define OLLAMA_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>

class Ollama : public QObject
{
    Q_OBJECT
public:
    explicit Ollama(QNetworkAccessManager *manager, QObject *parent = nullptr);
    ~Ollama();

    void checkServerAvailable(const QUrl &url, std::function<void(bool)> callback);
    void checkModelsAvailable(std::function<void(QStringList)> callback);
    void generate(const QString &prompt, const QString &model, std::function<void(QString)> callback);
    //void generateVision(const QString &prompt, const QString &model, std::function<void(QString)> callback);

signals:
    void responseReceived(const QString &response);

private:
    QNetworkAccessManager *m_manager;
    QUrl m_url;
};

#endif // OLLAMA_H
