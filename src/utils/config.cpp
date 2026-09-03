/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

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
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "config.h"
#include "logger.h"

namespace {
    constexpr auto kMetaKey      = QLatin1String("_meta");
    constexpr auto kActiveKey    = QLatin1String("active_profile");
    constexpr auto kDefaultName  = QLatin1String("Default");
    constexpr auto kSettingsFile = QLatin1String("settings.json");
}

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

    m_instance->m_configFilePath = configPath;
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
    const QString dirPath = m_instance->m_configFilePath + filename;
    if (!QFile::exists(dirPath)) {
        Log(Logger::Level::Warning, "[config] Failed to open file " + filename);
        return;
    }
    m_instance->m_isLoaded = true;
    m_instance->m_settings = readJsonFile(dirPath);
}

void Config::saveConfig(const QString& filename)
{
    writeJsonFile(m_instance->m_configFilePath + filename, m_instance->m_settings);
}

// Internal helpers

QString Config::profilesDirPath()
{
    return m_instance->m_configFilePath + QStringLiteral("configs/");
}

QString Config::profilePath(const QString& name)
{
    return profilesDirPath() + name + QStringLiteral(".json");
}

QJsonObject Config::readJsonFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QJsonObject();
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

void Config::writeJsonFile(const QString& path, const QJsonObject& obj)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        Log(Logger::Level::Warning, "[config] Failed to open file for writing: " + path);
        return;
    }
    file.write(QJsonDocument(obj).toJson());
    file.close();
}

bool Config::isProfileSection(const QString& key)
{
    return key != kMetaKey && !baseSections().contains(key);
}

QJsonObject Config::metaObject()
{
    return m_instance->m_settings.value(kMetaKey).toObject();
}

void Config::setMetaObject(const QJsonObject& meta)
{
    m_instance->m_settings[kMetaKey] = meta;
}

// Profile-aware persistence

const QStringList &Config::baseSections() // Global
{
    static const QStringList base = {
        QStringLiteral("general"),
        QStringLiteral("proxy"),
        QStringLiteral("output_window_geometry"),
        QStringLiteral("python"),
    };
    return base;
}

void Config::load()
{
    const QString settingsPath = m_instance->m_configFilePath + kSettingsFile;
    const bool settingsExists = QFile::exists(settingsPath);

    QJsonObject root = readJsonFile(settingsPath);
    m_instance->m_settings = root;

    if (!root.contains(kMetaKey)) {
        QJsonObject profileObj;
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (isProfileSection(it.key()))
                profileObj[it.key()] = it.value();
        }
        writeJsonFile(profilePath(kDefaultName), profileObj);

        QJsonObject meta;
        meta[kActiveKey] = QString(kDefaultName);
        setMetaObject(meta);
        save();
        m_instance->m_isLoaded = false;
        return;
    }

    const QString active = metaObject().value(kActiveKey).toString(kDefaultName);
    const bool profileExists = QFile::exists(profilePath(active));
    const QJsonObject profileObj = readJsonFile(profilePath(active));
    for (auto it = profileObj.begin(); it != profileObj.end(); ++it)
        m_instance->m_settings[it.key()] = it.value();

    m_instance->m_isLoaded = settingsExists && profileExists;
}

void Config::save()
{
    QJsonObject meta = metaObject();
    if (!meta.contains(kActiveKey)) meta[kActiveKey] = QString(kDefaultName);
    const QString active = meta.value(kActiveKey).toString(kDefaultName);

    QJsonObject baseRoot;
    QJsonObject profileObj;
    for (auto it = m_instance->m_settings.begin(); it != m_instance->m_settings.end(); ++it) {
        if (it.key() == kMetaKey)
            continue;
        if (baseSections().contains(it.key()))
            baseRoot[it.key()] = it.value();
        else
            profileObj[it.key()] = it.value();
    }
    baseRoot[kMetaKey] = meta;

    writeJsonFile(m_instance->m_configFilePath + kSettingsFile, baseRoot);
    writeJsonFile(profilePath(active), profileObj);
}

QStringList Config::availableProfiles()
{
    QDir dir(profilesDirPath());
    QStringList names;
    const QStringList files = dir.entryList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);
    for (const QString& f : files)
        names << QFileInfo(f).completeBaseName();
    return names;
}

QString Config::activeProfile()
{
    return metaObject().value(kActiveKey).toString(kDefaultName);
}

bool Config::loadProfile(const QString& name)
{
    if (!QFile::exists(profilePath(name)))
        return false;

    const QJsonObject profileObj = readJsonFile(profilePath(name));

    // Drop the current profile sections, then apply the loaded ones
    QStringList toRemove;
    for (auto it = m_instance->m_settings.begin(); it != m_instance->m_settings.end(); ++it) {
        if (isProfileSection(it.key()))
            toRemove << it.key();
    }

    for (const QString& key : toRemove)
        m_instance->m_settings.remove(key);

    for (auto it = profileObj.begin(); it != profileObj.end(); ++it)
        m_instance->m_settings[it.key()] = it.value();

    QJsonObject meta = metaObject();
    meta[kActiveKey] = name;
    setMetaObject(meta);

    save();
    return true;
}

void Config::saveCurrentAsProfile(const QString& name)
{
    QJsonObject meta = metaObject();
    meta[kActiveKey] = name;
    setMetaObject(meta);
    save();
}

bool Config::deleteProfile(const QString& name)
{
    if (name == activeProfile())
        return false;

    return QFile::remove(profilePath(name));
}

bool Config::renameProfile(const QString& oldName, const QString& newName)
{
    if (oldName == newName || newName.isEmpty())
        return false;

    if (QFile::exists(profilePath(newName)))
        return false;

    if (!QFile::rename(profilePath(oldName), profilePath(newName)))
        return false;

    QJsonObject meta = metaObject();
    if (meta.value(kActiveKey).toString() == oldName) {
        meta[kActiveKey] = newName;
        setMetaObject(meta);
        save();
    }
    return true;
}
