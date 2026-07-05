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

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "plugininterface.h"

#include <QObject>
#include <QMap>
#include <QPluginLoader>

class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager();

    struct PluginInfo {
        QString name;
        QString version;
        QString minAppVersion;
        QStringList dependencies;
        QString description;
        QString type;
        QString category;
        QString targetTitle;
        QString targetExecutable;
        QMap<QString, QString> archPaths;
        QString textMode = QStringLiteral("whole");
    };

    QList<PluginInfo> scanPlugins();
    void loadPlugins();
    void unloadPlugins();
    QStringList loadedPlugins() const;
    QObject* getPlugin(const QString &name);
    QMap<QString, QStringList> validateDependencies(const QList<PluginInfo>& plugins);

private:
    QList<PluginInfo> m_registry;
    QMap<QString, QPluginLoader*> m_loaded;
    QString m_currentLanguage;

    bool isVersionLess(const QString &a, const QString &b);

    bool loadPlugin(const QString &name);
    bool unloadPlugin(const QString &name);

    PluginInfo getQtPluginMeta(const QString &path);
    PluginInfo getCppPluginMeta(const QString &path);
    static void parseCommonMeta(const QJsonObject &obj, PluginInfo &info);
    QString getFileArch(const QString &path);
};

#endif // PLUGINMANAGER_H
