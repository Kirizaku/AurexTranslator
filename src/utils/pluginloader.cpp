/******************************************************************************
    Copyright (C) 2026 by Daniil Nabiulin

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

#include "pluginloader.h"
#include "metadatareader.h"
#include "logger.h"
#include "config.h"

#include <QPluginLoader>
#include <QJsonArray>
#include <QDir>
#include <QSet>

PluginManager::PluginManager(QObject *parent) : QObject(parent)
{
    QJsonObject general = Config::getValue("general").toJsonObject();
    m_currentLanguage = general["language"].toString();
    if (m_currentLanguage.isEmpty()) {
        m_currentLanguage = QLocale::system().name();
    }
}

PluginManager::~PluginManager()
{
    unloadPlugins();
}

QList<PluginManager::PluginInfo> PluginManager::scanPlugins()
{
    QDir pluginDir(Config::getConfigDirPath() + "plugins/");
    if (!pluginDir.exists()) {
        pluginDir.mkpath(".");
    }

    Log(Logger::Level::Info, QStringLiteral("[plugin-loader] Scanning for plugins"));

    for (const QString &file : pluginDir.entryList({"*.dll", "*.so"})) {
        QString path = pluginDir.absoluteFilePath(file);
        path = QDir::toNativeSeparators(path);
        PluginInfo info;

        QPluginLoader loader(path);
        if (!loader.metaData().isEmpty()) {
            info = getQtPluginMeta(path);
        } else {
            info = getCppPluginMeta(path);
        }

        if (!info.name.isEmpty()) {
            info.filePath = path;
            m_registry << info;
            Log(Logger::Level::Info, QStringLiteral("[plugin-loader] Registered: %1 (%2)").arg(info.name, file));
        } else {
            Log(Logger::Level::Warning, QStringLiteral("[plugin-loader] Metadata missing or invalid for plugin: %1").arg(file));
        }
    }

    QDir binDir(Config::getConfigDirPath() + "plugins/" + "bin/");
    QMap<QString, PluginInfo> uniquePlugins;

    if (binDir.exists()) {
        QFileInfoList entries = binDir.entryInfoList(QDir::Files | QDir::Executable | QDir::NoDotAndDotDot);

        for (const QFileInfo &entry : entries) {
            PluginInfo info = getCppPluginMeta(entry.absoluteFilePath());

            if (!info.name.isEmpty()) {
                info.filePath = entry.absoluteFilePath();
                if (!uniquePlugins.contains(info.name)) {
                    uniquePlugins[info.name] = info;
                }
            }
        }
        Log(Logger::Level::Info, QStringLiteral("[plugin-loader] Registering %1 unique binary plugin(s) from /bin").arg(uniquePlugins.size()));
        for (const PluginInfo &info : uniquePlugins) {
            m_registry << info;
        }
    }

    return m_registry;
}

void PluginManager::loadPlugins()
{
    for (const auto &info : m_registry) {
        if (info.targetTitle == "main-program") {
            loadPlugin(info.name);
        }
    }
}

void PluginManager::unloadPlugins()
{
    QList<QString> plugins = m_loaded.keys();
    for (int i = plugins.size() - 1; i >= 0; --i) {
        unloadPlugin(plugins[i]);
    }
    m_registry.clear();
}

QStringList PluginManager::loadedPlugins() const
{
    return m_loaded.keys();
}

QObject* PluginManager::getPlugin(const QString &name)
{
    if (!m_loaded.contains(name)) return nullptr;
    return m_loaded[name]->instance();
}

bool PluginManager::loadPlugin(const QString &name)
{
    for (const auto &info : m_registry) {
        if (info.name == name) {
            QPluginLoader *loader = new QPluginLoader(info.filePath, this);
            QObject *plugin = loader->instance();
            if (plugin) {
                m_loaded[name] = loader;

                auto currentPlugin = qobject_cast<PluginInterface*>(plugin);
                currentPlugin->setLanguage(m_currentLanguage);

                return true;
            }
            delete loader;
        }
    }
    return false;
}

bool PluginManager::unloadPlugin(const QString &name)
{
    if (!m_loaded.contains(name)) return false;

    QPluginLoader *loader = m_loaded[name];
    loader->unload();
    delete loader;
    m_loaded.remove(name);
    return true;
}

PluginManager::PluginInfo PluginManager::getQtPluginMeta(const QString &path)
{
    PluginInfo info;
    QPluginLoader loader(path);
    QJsonObject meta = loader.metaData();
    QJsonObject obj = meta["MetaData"].toObject();
    info.name        = obj["name"].toString();
    info.version     = obj["version"].toString();
    info.description = obj["description"].toString();
    info.type        = obj["type"].toString();

    if (obj.contains("dependencies") && obj["dependencies"].isArray()) {
        QJsonArray arr = obj["dependencies"].toArray();
        for (const QJsonValue &val : arr) {
            info.dependencies << val.toString();
        }
    }

    QJsonObject target = obj["target"].toObject();
    info.targetTitle  = target["title"].toString();
    info.targetExecutable  = target["executable"].toString();

    return info;
}

PluginManager::PluginInfo PluginManager::getCppPluginMeta(const QString &path)
{
    MetaDataReader loader(path);
    QJsonObject obj = loader.metaData();

    PluginInfo info;
    info.name        = obj["name"].toString();
    info.version     = obj["version"].toString();
    info.description = obj["description"].toString();
    info.type        = obj["type"].toString();

    if (obj.contains("dependencies") && obj["dependencies"].isArray()) {
        QJsonArray arr = obj["dependencies"].toArray();
        for (const QJsonValue &val : arr) {
            info.dependencies << val.toString();
        }
    }

    QJsonObject target = obj["target"].toObject();
    info.targetTitle  = target["title"].toString();
    info.targetExecutable  = target["executable"].toString();

    return info;
}

QMap<QString, QStringList> PluginManager::validateDependencies(const QList<PluginInfo>& plugins)
{
    QMap<QString, QStringList> errorsMap;

    QSet<QString> availableNames;
    for (const auto& p : plugins) {
        availableNames.insert(p.name);
    }

    for (const auto& plugin : plugins) {
        QStringList missingDeps;

        for (const QString& depName : plugin.dependencies) {
            if (!availableNames.contains(depName)) {
                missingDeps << depName;
            }
        }

        if (!missingDeps.isEmpty()) {
            errorsMap[plugin.name] = missingDeps;
        }
    }

    return errorsMap;
}
