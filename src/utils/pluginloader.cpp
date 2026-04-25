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
    m_registry.clear();
    Log(Logger::Level::Info, QStringLiteral("[plugin-loader] Scanning for plugins"));

    auto processFile = [&](const QString& path) {
        PluginInfo info;
        QPluginLoader loader(path);
        if (!loader.metaData().isEmpty()) {
            info = getQtPluginMeta(path);
        } else {
            info = getCppPluginMeta(path);
        }

        if (info.name.isEmpty()) {
            Log(Logger::Level::Warning,
                QStringLiteral("[plugin-loader] Metadata missing or invalid: %1").arg(path));
            return;
        }

        const QString arch = getFileArch(path);
        if (arch.isEmpty()) {
            Log(Logger::Level::Warning,
                QStringLiteral("[plugin-loader] Unknown architecture, skipping: %1").arg(path));
            return;
        }

        for (int i = 0; i < m_registry.size(); ++i) {
            if (m_registry[i].name == info.name) {
                m_registry[i].archPaths[arch] = path;
                Log(Logger::Level::Info,
                    QStringLiteral("[plugin-loader] Added %1 path for: %2").arg(arch, info.name));
                return;
            }
        }

        info.archPaths[arch] = path;
        m_registry << info;
        Log(Logger::Level::Info,
            QStringLiteral("[plugin-loader] Registered: %1 (%2)").arg(info.name, arch));
    };

    QDir pluginDir(Config::getConfigDirPath() + "plugins/");
    if (!pluginDir.exists())
        pluginDir.mkpath(".");

    const QStringList files = pluginDir.entryList({"*.dll", "*.so"}, QDir::Files);
    for (const QString& file : files) {
        processFile(QDir::toNativeSeparators(pluginDir.absoluteFilePath(file)));
    }

    QDir binDir(Config::getConfigDirPath() + "plugins/bin/");
    if (binDir.exists()) {
        const QFileInfoList entries = binDir.entryInfoList(QDir::Files | QDir::Executable | QDir::NoDotAndDotDot);
        for (const QFileInfo& entry : entries) {
            processFile(entry.absoluteFilePath());
        }
    }

    Log(Logger::Level::Info,
        QStringLiteral("[plugin-loader] Scan complete: %1 plugin(s) registered").arg(m_registry.size()));

    return m_registry;
}

void PluginManager::loadPlugins()
{
    const auto registry = m_registry;
    for (const auto& info : registry) {
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
    const auto registry = m_registry;
    for (const auto& info : registry) {
        if (info.name != name) continue;
#if defined(Q_PROCESSOR_X86_64)
        const QString arch = QStringLiteral("x64");
#else
        const QString arch = QStringLiteral("x86");
#endif
        const QString path = info.archPaths.value(arch);
        QPluginLoader* loader = new QPluginLoader(path, this);
        QObject* plugin = loader->instance();
        if (plugin) {
            m_loaded[name] = loader;
            qobject_cast<PluginInterface*>(plugin)->setLanguage(m_currentLanguage);
            return true;
        }
        delete loader;
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
    QJsonObject obj  = meta["MetaData"].toObject();
    info.name        = obj["name"].toString();
    info.version     = obj["version"].toString();
    info.description = obj["description"].toString();
    info.type        = obj["type"].toString();
    info.category    = obj["category"].toString();

    const QJsonArray arr = obj["dependencies"].toArray();
    for (const QJsonValue& val : arr) {
        info.dependencies << val.toString();
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
    info.category    = obj["category"].toString();

    const QJsonArray arr = obj["dependencies"].toArray();
    for (const QJsonValue& val : arr) {
        info.dependencies << val.toString();
    }

    QJsonObject target = obj["target"].toObject();
    info.targetTitle  = target["title"].toString();
    info.targetExecutable  = target["executable"].toString();

    return info;
}

QString PluginManager::getFileArch(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();

    QByteArray header = f.read(64);
    f.close();

    // ELF (Linux)
    if (header.size() >= 5 && header.startsWith("\x7F""ELF")) {
        return header[4] == 1 ? QStringLiteral("x86") : QStringLiteral("x64");
    }

    // PE (Windows)
    if (header.size() >= 0x40 && header.startsWith("MZ")) {
        uint32_t peOffset = *reinterpret_cast<const uint32_t*>(header.constData() + 0x3C);
        QFile f2(path);
        if (!f2.open(QIODevice::ReadOnly)) return QString();
        if (!f2.seek(peOffset + 4)) return QString();
        QByteArray machine = f2.read(2);
        f2.close();
        if (machine.size() < 2) return QString();
        uint16_t m = *reinterpret_cast<const uint16_t*>(machine.constData());
        if (m == 0x014C) return QStringLiteral("x86");
        if (m == 0x8664) return QStringLiteral("x64");
    }

    return QString();
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
