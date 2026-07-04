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

#ifndef CONFIG_H
#define CONFIG_H

#include <QJsonObject>
#include <QStringList>

class Config
{
public:
    static Config *instance();
    static void initInstance(const QString& configPath);
    static void destroyInstance();

    static void setValue(const QString& key, const QVariant& value);
    static QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant());
    static bool isLoaded() { return m_instance->m_isLoaded; }

    static QString getConfigDirPath() { return instance()->m_configFilePath; }

    static void loadConfig(const QString& filename);
    static void saveConfig(const QString& filename);

    static const QStringList &baseSections();

    static void load();
    static void save();

    // Named profile management (files in configs/)
    static QStringList availableProfiles();
    static QString activeProfile();
    static bool loadProfile(const QString& name);
    static void saveCurrentAsProfile(const QString& name);
    static bool deleteProfile(const QString& name);
    static bool renameProfile(const QString& oldName, const QString& newName);

private:
    Config();
    ~Config();

    static Config *m_instance;

    static QString profilesDirPath();
    static QString profilePath(const QString& name);
    static QJsonObject readJsonFile(const QString& path);
    static void writeJsonFile(const QString& path, const QJsonObject& obj);
    static bool isProfileSection(const QString& key);
    static QJsonObject metaObject();
    static void setMetaObject(const QJsonObject& meta);

    QString m_configFilePath;
    QJsonObject m_settings;
    bool m_isLoaded = false;
};

#endif // CONFIG_H
