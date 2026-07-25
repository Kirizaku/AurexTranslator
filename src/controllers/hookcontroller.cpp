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

#include "hookcontroller.h"

#include "src/utils/plugininterface.h"
#include "src/utils/logger.h"

HookController::HookController(QObject *parent)
    : QObject(parent) {}

void HookController::setPlugin(PluginInterface *plugin)
{
    if (m_hookPlugin && m_hookPlugin != plugin) {
        stop();
    }

    m_hookPlugin = plugin;

    if (!m_hookPlugin) return;

    connect(m_hookPlugin, &PluginInterface::pluginMessage,
            this, &HookController::infoMessage, Qt::UniqueConnection);
    connect(m_hookPlugin, &PluginInterface::currentOutput,
            this, &HookController::textReceived, Qt::UniqueConnection);
    connect(m_hookPlugin, &PluginInterface::processLost,
            this, &HookController::shouldClearResults, Qt::UniqueConnection);
}

void HookController::setMode(int mode)
{
    m_hookMode = mode;
}

void HookController::setCurrentGameAppPlugin(const QString &name)
{
    m_currentGameAppPlugin = name;
}

void HookController::setCurrentEnginePlugin(const QString &name)
{
    m_currentEnginePlugin = name;
}

void HookController::setCurrentEngineProcess(const QString &processName)
{
    m_currentEngineProcess = processName;
}

void HookController::setRegistry(const QList<PluginManager::PluginInfo> &registry)
{
    m_registry = registry;
}

void HookController::setPluginConfig(const QString &pluginName, const QString &json)
{
    m_pluginConfig[pluginName] = json;

    // If this plugin is the one currently running, apply the change live
    if (pluginName == m_currentRunningPlugin)
        pushPluginConfig(pluginName);
}

void HookController::pushPluginConfig(const QString &pluginName)
{
    if (!m_hookPlugin) return;

    const QString json = m_pluginConfig.value(pluginName);
    if (json.isEmpty()) return;

    m_hookPlugin->execute(QStringLiteral("config"), { pluginName, json });
}

void HookController::apply(bool enabled, bool widgetReady)
{
    if (!m_hookPlugin) return;

    if (enabled && widgetReady) {
        // Engine mode requires a target process name
        if (m_hookMode == EngineMode && m_currentEngineProcess.isEmpty()) {
            if (!m_currentRunningPlugin.isEmpty()) stop();
            const QString msg = tr("[Hook] No process selected. Please choose a process in the settings");
            Log(Logger::Level::Warning, msg);
            emit infoMessage(msg);
            return;
        }

        // Find the configured plugin in the registry
        auto pluginIt = std::find_if(m_registry.cbegin(), m_registry.cend(),
                                     [this](const PluginManager::PluginInfo& info) {
                                         return (m_hookMode == GameAppMode)
                                                    ? (info.name == m_currentGameAppPlugin)
                                                    : (info.name == m_currentEnginePlugin);
                                     });

        if (pluginIt == m_registry.cend()) {
            if (!m_currentRunningPlugin.isEmpty()) stop();
            const QString msg = tr("[Hook] No game selected. Please choose a game in the settings");
            Log(Logger::Level::Warning, msg);
            emit infoMessage(msg);
            return;
        }

        // If something else is already running, stop it before starting a new one
        if (!m_currentRunningPlugin.isEmpty()) {
            stop();
        }
        startPlugin(*pluginIt);
    } else {
        stop();
    }
}

void HookController::manualInject()
{
    if (!m_hookPlugin || m_currentRunningPlugin.isEmpty()) return;

    auto pluginIt = std::find_if(m_registry.cbegin(), m_registry.cend(),
                                 [this](const PluginManager::PluginInfo& info) {
                                     return info.name == m_currentRunningPlugin;
                                 });

    if (pluginIt == m_registry.cend()) {
        Log(Logger::Level::Warning,
            QString("[Hook] Manual inject: plugin '%1' not found in registry").arg(m_currentRunningPlugin));
        return;
    }

    stop();
    startPlugin(*pluginIt);
}

void HookController::stop()
{
    if (!m_hookPlugin) return;

    m_hookPlugin->execute(QStringLiteral("stop"), { QString(), QString() });

    emit shouldClearResults();
    emit hookStateChanged(false);
    emit shouldClearInfoMessage();

    m_currentRunningPlugin.clear();
    m_runningEngineProcess.clear();
}

void HookController::retarget(bool enabled)
{
    if (!enabled) {
        stop();
        return;
    }

    const QString desiredPlugin = (m_hookMode == GameAppMode)
                                      ? m_currentGameAppPlugin
                                      : m_currentEnginePlugin;

    const bool sameTarget = !m_currentRunningPlugin.isEmpty()
                            && desiredPlugin == m_currentRunningPlugin
                            && (m_hookMode != EngineMode
                            || m_currentEngineProcess == m_runningEngineProcess);

    if (sameTarget)
        return;

    stop();
    apply(true, true);
}

void HookController::startPlugin(const PluginManager::PluginInfo& info)
{
    QString exe;
    if (m_hookMode == GameAppMode) {
        exe = info.targetExecutable;
    } else {
        exe = m_currentEngineProcess;
    }

    QStringList archList;
    for (auto it = info.archPaths.constBegin(); it != info.archPaths.constEnd(); ++it) {
        archList << (it.key() + QStringLiteral(":") + it.value());
    }

    m_hookPlugin->execute(QStringLiteral("start"), { exe, info.name, archList.join(QStringLiteral(";")), info.textMode });

    pushPluginConfig(info.name);

    m_currentRunningPlugin = info.name;
    m_runningEngineProcess = (m_hookMode == EngineMode) ? m_currentEngineProcess : QString();

    emit hookStateChanged(true);
}
