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

#include <QJsonDocument>

#include "config.h"
#include "logger.h"

Config *Config::m_instance = nullptr;
Config *Config::instance()
{
    return m_instance;
}

Config::Config() {}
Config::~Config() {}

void Config::initInstance(const QString& configPath)
{
    if (!m_instance) {
        m_instance = new Config();
    }

    m_instance->m_logFilePath = configPath;
}

void Config::destroyInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void Config::setValue(const QString& key, const QVariant& value)
{
    m_instance->m_settings[key] = QJsonValue::fromVariant(value);
}

QVariant Config::getValue(const QString& key, const QVariant& defaultValue)
{
    if (m_instance->m_settings.contains(key)) {
        return m_instance->m_settings[key].toVariant();
    }
    return defaultValue;
}

void Config::loadConfig(const QString& filename)
{
    QString dirPath = m_instance->m_logFilePath + filename;

    QFile file(dirPath);
    if (!file.open(QIODevice::ReadOnly)) {
        Log(Logger::Level::Warning, "[config] Failed to open file settings.json");
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    m_instance->m_settings = doc.isObject() ? doc.object() : QJsonObject();
}

void Config::saveConfig(const QString& filename)
{
    QString dirPath = m_instance->m_logFilePath + filename;

    QFile file(dirPath);
    if (!file.open(QIODevice::WriteOnly)) {
        Log(Logger::Level::Warning, "[config] Failed to open file settings.json for writing");
        return;
    }

    QJsonDocument doc(m_instance->m_settings);
    file.write(doc.toJson());
    file.close();
}
