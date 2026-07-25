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

#ifndef HOOKCONTROLLER_H
#define HOOKCONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>

#include "src/utils/pluginloader.h"

class PluginInterface;

class HookController : public QObject
{
    Q_OBJECT

public:
    enum Mode {
        GameAppMode = 0,
        EngineMode  = 1
    };

    explicit HookController(QObject *parent = nullptr);

    void setPlugin(PluginInterface *plugin);
    bool isPluginLoaded() const { return m_hookPlugin != nullptr; }

    // Settings
    void setMode(int mode);
    void setCurrentGameAppPlugin(const QString &name);
    void setCurrentEnginePlugin(const QString &name);
    void setCurrentEngineProcess(const QString &processName);
    void setRegistry(const QList<PluginManager::PluginInfo> &registry);
    void setPluginConfig(const QString &pluginName, const QString &json);

    // State
    QString currentRunningPlugin() const { return m_currentRunningPlugin; }
    QString runningEngineProcess() const { return m_runningEngineProcess; }
    bool isRunning() const { return !m_currentRunningPlugin.isEmpty(); }
    void retarget(bool enabled);

public slots:
    void apply(bool enabled, bool widgetReady);

    // Manual re-inject
    void manualInject();

    // Stop the current hook plugin if any is running
    void stop();

signals:
    // Plugin reports a text snippet (forwarded from PluginInterface::currentOutput)
    void textReceived(const QString &source, const QString &text);

    // Plugin status/info message
    void infoMessage(const QString &message);

    // Hook state changed (active/inactive)
    void hookStateChanged(bool active);

    void shouldClearResults();
    void shouldClearInfoMessage();

private:
    PluginInterface *m_hookPlugin = nullptr;

    int m_hookMode = GameAppMode;
    QString m_currentGameAppPlugin;
    QString m_currentEnginePlugin;
    QString m_currentEngineProcess;
    QList<PluginManager::PluginInfo> m_registry;
    QMap<QString, QString> m_pluginConfig;

    QString m_currentRunningPlugin;
    QString m_runningEngineProcess;

    void startPlugin(const PluginManager::PluginInfo& info);
    void pushPluginConfig(const QString &pluginName);
};

#endif // HOOKCONTROLLER_H
