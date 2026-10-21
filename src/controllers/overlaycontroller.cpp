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

#include "overlaycontroller.h"
#include "overlaycaptureserver.h"

#include "src/utils/plugininterface.h"
#include "src/utils/config.h"
#include "src/utils/overlay_protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QSettings>
#endif

#ifdef Q_OS_LINUX
#include <QStandardPaths>
#endif

const QString OverlayController::kPluginName = QStringLiteral("libat-overlay");

namespace {

QString pluginsDirPath()
{
    return Config::getConfigDirPath() + QStringLiteral("plugins/");
}

#ifdef Q_OS_WIN
QString vkTargetsFilePath()
{
    return pluginsDirPath() + QStringLiteral(AT_OVERLAY_VK_TARGETS_FILE);
}

QString managedGamesFilePath()
{
    return pluginsDirPath() + QStringLiteral("at-overlay-managed-games.json");
}

QString vklayerDllPath(const QString &arch)
{
    return pluginsDirPath() + QStringLiteral("libat-overlay_") + arch + QStringLiteral(".dll");
}

QString vklayerManifestPath(const QString &arch)
{
    return pluginsDirPath() + QStringLiteral("bin/at-overlay-vklayer") + arch + QStringLiteral(".json");
}

bool registerVulkanLayerArch(const QString &arch)
{
    const QString dllPath = vklayerDllPath(arch);
    if (!QFile::exists(dllPath))
        return false;

    QJsonObject layer;
    layer[QStringLiteral("name")] = QStringLiteral("VK_LAYER_at_overlay_translator");
    layer[QStringLiteral("type")] = QStringLiteral("GLOBAL");
    layer[QStringLiteral("library_path")] = dllPath;
    layer[QStringLiteral("api_version")] = QStringLiteral("1.3.0");
    layer[QStringLiteral("implementation_version")] = QStringLiteral("1");
    layer[QStringLiteral("description")] = QStringLiteral("at-overlay in-game translation overlay");

    QJsonObject functions;
    functions[QStringLiteral("vkNegotiateLoaderLayerInterfaceVersion")] = QStringLiteral("vkNegotiateLoaderLayerInterfaceVersion");
    layer[QStringLiteral("functions")] = functions;

    QJsonObject disableEnv;
    disableEnv[QStringLiteral("AT_OVERLAY_VK_DISABLE")] = QStringLiteral("1");
    layer[QStringLiteral("disable_environment")] = disableEnv;

    QJsonObject root;
    root[QStringLiteral("file_format_version")] = QStringLiteral("1.2.0");
    root[QStringLiteral("layer")] = layer;

    const QString manifestPath = vklayerManifestPath(arch);
    QDir().mkpath(pluginsDirPath() + QStringLiteral("bin/"));
    QFile manifestFile(manifestPath);

    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    manifestFile.close();

    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Khronos\\Vulkan\\ImplicitLayers"), QSettings::NativeFormat);

    settings.setValue(QDir::toNativeSeparators(manifestPath), 0);
    return settings.status() == QSettings::NoError;
}

QStringList readVkTargets()
{
    QStringList lines;
    QFile f(vkTargetsFilePath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty()) lines << line;
        }
    }
    return lines;
}

void writeVkTargets(const QStringList &lines)
{
    if (lines.isEmpty()) {
        QFile::remove(vkTargetsFilePath());
        return;
    }
    QFile f(vkTargetsFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QTextStream out(&f);
    for (const QString &line : lines) out << line << Qt::endl;
}

void addVkTarget(const QString &exe)
{
    QStringList lines = readVkTargets();
    if (!lines.contains(exe, Qt::CaseInsensitive)) lines << exe;
    writeVkTargets(lines);
}

void removeVkTarget(const QString &exe)
{
    QStringList lines = readVkTargets();
    for (auto it = lines.begin(); it != lines.end(); ) {
        if (it->compare(exe, Qt::CaseInsensitive) == 0) it = lines.erase(it);
        else ++it;
    }
    writeVkTargets(lines);
}

bool registerVulkanLayer()
{
    bool any = false;
    for (const QString &arch : { QStringLiteral("x64"), QStringLiteral("x86") }) {
        if (registerVulkanLayerArch(arch)) any = true;
    }
    return any;
}

bool unregisterVulkanLayerArch(const QString &arch)
{
    const QString manifestPath = vklayerManifestPath(arch);
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Khronos\\Vulkan\\ImplicitLayers"),
                       QSettings::NativeFormat);
    settings.remove(QDir::toNativeSeparators(manifestPath));

    const bool existed = QFile::exists(manifestPath);
    QFile::remove(manifestPath);
    return existed;
}
#endif

#ifdef Q_OS_LINUX
QString linuxManifestPath(const QString &arch)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/vulkan/implicit_layer.d/at-overlay-vulkan_")
           + arch + QStringLiteral(".json");
}

bool registerVulkanLayerLinuxArch(const QString &arch)
{
    const QString soPath = pluginsDirPath() + QStringLiteral("libat-overlay-vulkan_") + arch + QStringLiteral(".so");
    if (!QFile::exists(soPath))
        return false;

    QJsonObject layer;
    layer[QStringLiteral("name")] = QStringLiteral("VK_LAYER_AT_overlay");
    layer[QStringLiteral("type")] = QStringLiteral("GLOBAL");
    layer[QStringLiteral("library_path")] = soPath;
    layer[QStringLiteral("library_arch")] = (arch == QStringLiteral("x64")) ? QStringLiteral("64") : QStringLiteral("32");
    layer[QStringLiteral("api_version")] = QStringLiteral("1.3.0");
    layer[QStringLiteral("implementation_version")] = QStringLiteral("1");
    layer[QStringLiteral("description")] = QStringLiteral("at-overlay in-game translation overlay");

    QJsonObject functions;
    functions[QStringLiteral("vkNegotiateLoaderLayerInterfaceVersion")] = QStringLiteral("AT_Negotiate");
    layer[QStringLiteral("functions")] = functions;

    QJsonObject enableEnv;
    enableEnv[QStringLiteral("AT_OVERLAY_ENABLE")] = QStringLiteral("1");
    layer[QStringLiteral("enable_environment")] = enableEnv;

    QJsonObject disableEnv;
    disableEnv[QStringLiteral("AT_OVERLAY_DISABLE")] = QStringLiteral("1");
    layer[QStringLiteral("disable_environment")] = disableEnv;

    QJsonObject root;
    root[QStringLiteral("file_format_version")] = QStringLiteral("1.1.2");
    root[QStringLiteral("layer")] = layer;

    const QString manifestPath = linuxManifestPath(arch);
    QDir().mkpath(QFileInfo(manifestPath).absolutePath());
    QFile dst(manifestPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    dst.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool registerVulkanLayerLinux()
{
    bool any = false;
    for (const QString &arch : { QStringLiteral("x64"), QStringLiteral("x86") }) {
        if (registerVulkanLayerLinuxArch(arch)) any = true;
    }
    return any;
}

QString launcherScriptPathLinux()
{
    return Config::getConfigDirPath() + QStringLiteral("at-overlay");
}

QString preloadDirPathLinux()
{
    return pluginsDirPath() + QStringLiteral("preload/");
}

bool writeLauncherScriptLinux()
{
    const QList<QPair<QString, QString>> platforms = {
        { QStringLiteral("x86_64"), QStringLiteral("libat-overlay-shim_x64.so") },
        { QStringLiteral("i686"),   QStringLiteral("libat-overlay-shim_x86.so") },
    };

    bool any = false;
    for (const auto &[platform, shim] : platforms) {
        const QString target = pluginsDirPath() + shim;
        const QString link = preloadDirPathLinux() + platform + QStringLiteral("/libat-overlay-shim.so");

        QFile::remove(link);
        if (!QFile::exists(target)) continue;
        QDir().mkpath(preloadDirPathLinux() + platform);
        if (QFile::link(target, link)) any = true;
    }
    if (!any) return false;

    const QString scriptPath = launcherScriptPathLinux();
    const QString script = QStringLiteral(
        "#!/bin/sh\n"
        "# at-overlay launcher, generated by AurexTranslator.\n"
        "# Usage: %1 %command%   (Steam: Properties -> Launch Options)\n"
        "\n"
        "export AT_OVERLAY_ENABLE=1\n"
        "LD_PRELOAD=\"${LD_PRELOAD:+$LD_PRELOAD:}%2\\$PLATFORM/libat-overlay-shim.so\"\n"
        "export LD_PRELOAD\n"
        "exec \"$@\"\n"
    ).arg(scriptPath, preloadDirPathLinux());

    QFile scriptFile(scriptPath);

    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    scriptFile.write(script.toUtf8());
    scriptFile.close();

    return QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                             QFile::ReadGroup | QFile::ExeGroup |
                                             QFile::ReadOther | QFile::ExeOther);
}
#endif

} // namespace

OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
{
    m_captureServer = new OverlayCaptureServer;
    m_captureServer->moveToThread(&m_captureThread);
    connect(&m_captureThread, &QThread::started, m_captureServer, &OverlayCaptureServer::start);
    connect(&m_captureThread, &QThread::finished, m_captureServer, &QObject::deleteLater);
    connect(m_captureServer, &OverlayCaptureServer::clientListChanged, this, [this](const QStringList &exeNames, const QList<quint32> &pids) {
        m_gameNames = exeNames;
        m_gamePids = pids;
        emit gameListChanged(exeNames, pids);
    });
    connect(m_captureServer, &OverlayCaptureServer::activeClientChanged, this, [this](const QString &exe, quint32 pid) {
        announceActiveGame(exe, pid);
        emit sourceDisconnected();
    });
    m_captureThread.start();
}

OverlayController::~OverlayController()
{
    if (m_captureServer) m_captureServer->stop();
    m_captureThread.quit();
    m_captureThread.wait();
}

void OverlayController::setPlugin(PluginInterface *plugin)
{
    if (m_injectorPlugin == plugin) return;
    if (m_injectorPlugin)
        stop();

    m_injectorPlugin = plugin;

    if (!m_injectorPlugin) return;

    connect(m_injectorPlugin, &PluginInterface::pluginMessage, this,
            [this](const QString &pluginName, const QString &msg) {
        if (pluginName == kPluginName) emit infoMessage(msg);
    });
    connect(m_injectorPlugin, &PluginInterface::processFound, this,
            [this](const QString &pluginName, const QString &processName) {
        if (pluginName == kPluginName) onGameProcessFound(processName);
    });
    connect(m_injectorPlugin, &PluginInterface::processLost, this,
            [this](const QString &pluginName, const QString &processName) {
        if (pluginName == kPluginName) onGameProcessLost(processName);
    });
}

void OverlayController::setRegistry(const QList<PluginManager::PluginInfo> &registry)
{
    m_registry = registry;
}

void OverlayController::replayGameState()
{
    if (!m_gameNames.isEmpty())
        emit gameListChanged(m_gameNames, m_gamePids);
    
    if (!m_activeGameExe.isEmpty())
        announceActiveGame(m_activeGameExe, m_activeGamePid);
}

QString OverlayController::installVulkanSupport(bool *ok)
{
    if (ok) *ok = false;

#ifdef Q_OS_WIN
    if (!registerVulkanLayer()) {
        return tr("The overlay plugin isn't deployed to %1 - "
                  "reinstall AurexTranslator.").arg(pluginsDirPath());
    }

    if (ok) *ok = true;
    return tr("Vulkan layer registered. Enable Overlay capture or check "
              "\"In-game overlay\" in Output settings, then click \"Add "
              "game...\" to pick a running process.");
#elif defined(Q_OS_LINUX)
    const bool vulkanOk = registerVulkanLayerLinux();
    const bool launcherOk = writeLauncherScriptLinux();
    if (!vulkanOk && !launcherOk) {
        return tr("The overlay plugin isn't deployed to %1 - "
                  "reinstall AurexTranslator.").arg(pluginsDirPath());
    }

    if (ok) *ok = true;
    if (vulkanOk && launcherOk) {
        return tr("Vulkan layer and launcher installed. Use the command below as "
                  "your game's launch command (e.g. Steam Launch Options).");
    }
    if (vulkanOk) {
        return tr("Vulkan layer registered, but the launcher script couldn't be "
                  "created because the overlay launcher plugin isn't deployed. "
                  "Without it the layer only activates for games started with "
                  "AT_OVERLAY_ENABLE=1.");
    }
    return tr("Launcher installed. Use the command below as your "
              "game's launch command (e.g. Steam Launch Options) - the "
              "Vulkan layer plugin isn't deployed, so Vulkan games won't "
              "get an overlay.");
#else
    return tr("Overlay mode is not supported on this platform yet");
#endif
}

QString OverlayController::uninstallVulkanSupport()
{
#ifdef Q_OS_WIN
    ensureKnownGamesLoaded();

    bool foundAny = false;
    for (const QString &arch : { QStringLiteral("x64"), QStringLiteral("x86") }) {
        if (unregisterVulkanLayerArch(arch)) foundAny = true;
    }

    const bool hadLiveTarget = !m_currentGame.isEmpty();
    if (hadLiveTarget) removeVkTarget(m_currentGame);

    if (!foundAny)
        return tr("Nothing to uninstall - the Vulkan layer was not registered.");
    return hadLiveTarget ? tr("Vulkan layer unregistered. Its panel will disappear from the "
                              "currently running game within a couple of seconds, but the "
                              "layer itself stays loaded in that game's memory until it's "
                              "restarted - it just won't do anything there anymore, and no "
                              "other game will pick it up at all.")
                         : tr("Vulkan layer unregistered.");
#elif defined(Q_OS_LINUX)
    const QString scriptPath = launcherScriptPathLinux();
    QDir(preloadDirPathLinux()).removeRecursively();

    bool foundAny = false;
    for (const QString &arch : { QStringLiteral("x64"), QStringLiteral("x86") }) {
        if (QFile::remove(linuxManifestPath(arch)))
            foundAny = true;
    }
    if (QFile::exists(scriptPath)) {
        QFile::remove(scriptPath);
        foundAny = true;
    }
    return foundAny ? tr("Vulkan/OpenGL launcher uninstalled.")
                    : tr("Nothing to uninstall - the launcher was not installed.");
#else
    return tr("Overlay mode is not supported on this platform yet");
#endif
}

bool OverlayController::isVulkanLayerRegistered() const
{
#ifdef Q_OS_WIN
    for (const QString &arch : { QStringLiteral("x64"), QStringLiteral("x86") }) {
        if (QFile::exists(vklayerManifestPath(arch)))
            return true;
    }
    return false;
#elif defined(Q_OS_LINUX)
    return QFile::exists(linuxManifestPath(QStringLiteral("x64")))
        || QFile::exists(linuxManifestPath(QStringLiteral("x86")));
#else
    return false;
#endif
}

QString OverlayController::launcherPath() const
{
#ifdef Q_OS_LINUX
    const QString launcher = launcherScriptPathLinux();
    return QFile::exists(launcher) ? launcher : QString();
#else
    return QString();
#endif
}

void OverlayController::setFrameCaptureAllowed(bool allowed)
{
    m_captureServer->setFrameCaptureAllowed(allowed);
}

void OverlayController::setShowOriginal(bool show)
{
    m_showOriginal = show;
    recomposeAndPush();
}

void OverlayController::setShowSource(bool show)
{
    m_showSource = show;
    recomposeAndPush();
}

void OverlayController::setShowTranslatorName(bool show)
{
    m_showTranslatorName = show;
    recomposeAndPush();
}

void OverlayController::setStyle(const QColor &textColor, const QColor &bgColor, float fontSize,
                                 float zoneX, float zoneY, float zoneW, float zoneH, bool fontSizeAuto,
                                 int cornerRadius, Qt::Alignment align,
                                 int marginTop, int marginBottom, int marginLeft, int marginRight)
{
    m_captureServer->setStyle(textColor, bgColor, fontSize, zoneX, zoneY, zoneW, zoneH, fontSizeAuto,
                              cornerRadius, align, marginTop, marginBottom, marginLeft, marginRight);
}

void OverlayController::setFontPath(const QString &path)
{
    m_captureServer->setFontPath(path);
}

void OverlayController::restorePersistedGames()
{
#ifdef Q_OS_WIN
    ensureKnownGamesLoaded();
    if (!m_currentGame.isEmpty() && m_knownGames.contains(m_currentGame)) {
        if (!startManaging(m_currentGame)) {
            m_currentGame.clear();
            saveKnownGames();
            emitKnownGamesChanged();
        }
    }
#endif
}

void OverlayController::ensureKnownGamesLoaded()
{
#ifdef Q_OS_WIN
    if (m_knownGamesLoaded) return;
    loadKnownGames();
    m_knownGamesLoaded = true;
    emitKnownGamesChanged();
#endif
}

void OverlayController::addGame(const QString &exe)
{
#ifdef Q_OS_WIN
    if (exe.isEmpty()) return;
    m_knownGames.insert(exe);
    saveKnownGames();
    switchToGame(exe);
#else
    Q_UNUSED(exe);
#endif
}

void OverlayController::removeGame(const QString &exe)
{
#ifdef Q_OS_WIN
    if (!m_knownGames.contains(exe)) return;
    m_knownGames.remove(exe);

    if (exe == m_currentGame) {
        auto it = m_managedGames.find(exe);
        if (it != m_managedGames.end()) {
            stopGameSession(exe);
            m_managedGames.erase(it);
        }
        m_currentGame.clear();
        emitManagedGamesChanged();

        if (m_running) {
            m_captureServer->setActive(false);

            m_entries.clear();

            m_running = false;
        }
    }

    saveKnownGames();
    emitKnownGamesChanged();
#else
    Q_UNUSED(exe);
#endif
}

void OverlayController::switchToGame(const QString &exe)
{
#ifdef Q_OS_WIN
    if (exe.isEmpty() || !m_knownGames.contains(exe) || exe == m_currentGame) return;

    if (!m_currentGame.isEmpty()) {
        auto it = m_managedGames.find(m_currentGame);
        if (it != m_managedGames.end()) {
            stopGameSession(m_currentGame);
            m_managedGames.erase(it);
        }
    }

    m_currentGame = exe;
    if (!startManaging(exe))
        m_currentGame.clear();

    saveKnownGames();
    emitKnownGamesChanged();
#else
    Q_UNUSED(exe);
#endif
}

void OverlayController::unloadAllGames()
{
#ifdef Q_OS_WIN
    for (auto it = m_managedGames.cbegin(); it != m_managedGames.cend(); ++it)
        stopGameSession(it.key());

    m_managedGames.clear();
    emitManagedGamesChanged();
    emitKnownGamesChanged();

    m_entries.clear();

    if (m_running) {
        m_captureServer->setActive(false);
        m_running = false;
    }
#endif
}

void OverlayController::start()
{
#ifdef Q_OS_LINUX
    if (m_running) return;

    m_captureServer->setActive(true);
    m_running = true;
#endif
}

void OverlayController::stop()
{
#ifdef Q_OS_LINUX
    if (!m_running) return;

    m_captureServer->setActive(false);
    m_entries.clear();
    m_running = false;
#endif
}

void OverlayController::setPreviewActive(bool active)
{
    if (m_previewActive == active) return;
    m_previewActive = active;
    recomposeAndPush();
}

void OverlayController::setActiveGame(const QString &exe, quint32 pid)
{
    m_captureServer->setActiveClient(exe, pid);
}

void OverlayController::setTextDisplayEnabled(bool enabled)
{
    if (m_textDisplayEnabled == enabled) return;
    m_textDisplayEnabled = enabled;
    if (enabled) recomposeAndPush();
    else m_captureServer->setText(QString(), QString(), false);
}

void OverlayController::setHiddenByHotkey(bool hidden)
{
    if (m_hiddenByHotkey == hidden) return;
    m_hiddenByHotkey = hidden;
    emit hiddenChanged(hidden);
    recomposeAndPush();
}

void OverlayController::setTranslationResult(const QString &source, const QString &translatorName, const QString &original, const QString &result)
{
    TranslationEntry entry;
    entry.source = source;
    entry.translatorName = translatorName;
    entry.original = original;
    entry.result = result;

    bool updated = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        TranslationEntry &existing = m_entries[i];
        if (existing.translatorName == translatorName && existing.source == source) {
            if (result.isEmpty()) {
                m_entries.removeAt(i);
            } else {
                existing = entry;
            }
            updated = true;
            break;
        }
    }

    if (!updated && !result.isEmpty())
        m_entries.append(entry);

    recomposeAndPush();
}

void OverlayController::setOriginalText(const QString &source, const QString &original, bool translationPending)
{
    if (original.isEmpty()) return;

    const bool showNow = !translationPending || m_showOriginal;

    bool refreshed = false;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        TranslationEntry &existing = m_entries[i];
        if (existing.source != source)
            continue;

        if (existing.translatorName.isEmpty()) {
            if (refreshed) {
                m_entries.removeAt(i);
                continue;
            }
            existing.original = original;
            refreshed = true;
        } else if (showNow && (!translationPending || existing.original != original)) {
            m_entries.removeAt(i);
        }
    }

    if (!refreshed) {
        TranslationEntry entry;
        entry.source = source;
        entry.original = original;
        m_entries.append(entry);
    }

    recomposeAndPush();
}

void OverlayController::clearResultsBySource(const QString &source)
{
    const bool isHookGroup = (source == QStringLiteral("Hook"));
    bool changed = false;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const QString &entrySource = m_entries[i].source;
        const bool match = isHookGroup ? entrySource.startsWith(QStringLiteral("Hook"))
                                       : (entrySource == source);
        if (match) {
            m_entries.removeAt(i);
            changed = true;
        }
    }
    if (changed) recomposeAndPush();
}

void OverlayController::clearResultsByTranslator(const QString &translatorName)
{
    bool changed = false;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries[i].translatorName == translatorName) {
            m_entries.removeAt(i);
            changed = true;
        }
    }
    if (changed) recomposeAndPush();
}

void OverlayController::announceActiveGame(const QString &exe, quint32 pid)
{
    m_activeGameExe = exe;
    m_activeGamePid = pid;
    emit infoMessage(exe.isEmpty() ? tr("[Overlay] No game active")
                                   : tr("[Overlay] Active game: %1").arg(exe));
    emit activeGameChanged(exe, pid);
}

void OverlayController::pushText(const QString &text)
{
    if (m_hiddenByHotkey) {
        m_captureServer->setText(QString(), QString(), false);
        return;
    }
    if (!m_previewActive && !m_textDisplayEnabled) return;
    m_captureServer->setText(QString(), text, !text.isEmpty());
}

void OverlayController::recomposeAndPush()
{
    pushText(composeText());
}

QString OverlayController::composeText() const
{
    QSet<QString> translated;
    for (const auto &entry : m_entries) {
        if (!entry.result.isEmpty())
            translated.insert(entry.source + QChar(0x1f) + entry.original);
    }

    QStringList blocks;
    for (const auto &entry : m_entries) {
        if (entry.result.isEmpty() && (!m_showOriginal || entry.original.isEmpty()))
            continue;
        if (entry.result.isEmpty() && translated.contains(entry.source + QChar(0x1f) + entry.original))
            continue;

        QStringList parts;
        if (m_showSource && !entry.source.isEmpty())
            parts << QStringLiteral("[%1]").arg(entry.source);
        if (m_showTranslatorName && !entry.translatorName.isEmpty())
            parts << QStringLiteral("[%1]").arg(entry.translatorName);
        QString header = parts.join(QStringLiteral(" "));
        if (!header.isEmpty()) header += QStringLiteral("\n");

        QString originalPart;
        if (m_showOriginal && !entry.original.isEmpty()) {
            originalPart = entry.original;
            if (!entry.result.isEmpty()) originalPart += QStringLiteral("\n\n");
        }

        blocks << header + originalPart + entry.result;
    }

    return blocks.join(QStringLiteral("\n\n"));
}

void OverlayController::emitManagedGamesChanged()
{
    QStringList searchingGames, injectingGames;
    for (auto it = m_managedGames.cbegin(); it != m_managedGames.cend(); ++it) {
        if (it.value().processFound) injectingGames << it.key();
        else searchingGames << it.key();
    }
    emit managedGamesChanged(searchingGames, injectingGames);
}

void OverlayController::emitKnownGamesChanged()
{
    QStringList others;
    for (const QString &exe : m_knownGames) {
        if (!m_managedGames.contains(exe)) others << exe;
    }
    emit knownGamesChanged(others);
}

void OverlayController::onGameProcessFound(const QString &processName)
{
#ifdef Q_OS_WIN
    auto it = m_managedGames.find(processName);
    if (it == m_managedGames.end() || it.value().processFound) return;

    it.value().processFound = true;
    emitManagedGamesChanged();
#else
    Q_UNUSED(processName);
#endif
}

void OverlayController::onGameProcessLost(const QString &processName)
{
#ifdef Q_OS_WIN
    auto it = m_managedGames.find(processName);
    if (it == m_managedGames.end() || !it.value().processFound) return;

    it.value().processFound = false;
    emitManagedGamesChanged();
#else
    Q_UNUSED(processName);
#endif
}

void OverlayController::stopGameSession(const QString &exe)
{
#ifdef Q_OS_WIN
    removeVkTarget(exe);
    if (m_injectorPlugin) {
        m_injectorPlugin->execute(QStringLiteral("stop"), { kPluginName, exe });
    }
#else
    Q_UNUSED(exe);
#endif
}

bool OverlayController::startManaging(const QString &exe)
{
#ifdef Q_OS_WIN
    if (!m_injectorPlugin) return false;

    auto pluginIt = std::find_if(m_registry.cbegin(), m_registry.cend(),
                                 [](const PluginManager::PluginInfo &info) {
                                     return info.name == kPluginName;
                                 });
    if (pluginIt == m_registry.cend()) {
        emit infoMessage(tr("[Overlay] Plugin 'libat-overlay' is not available"));
        return false;
    }

    QStringList archList;
    for (auto it = pluginIt->archPaths.constBegin(); it != pluginIt->archPaths.constEnd(); ++it) {
        archList << (it.key() + QStringLiteral(":") + it.value());
    }

    addVkTarget(exe);
    m_injectorPlugin->execute(QStringLiteral("start"),
                              { exe, kPluginName, archList.join(QStringLiteral(";")), QString() });

    m_managedGames.insert(exe, ManagedGame());
    emitManagedGamesChanged();

    if (!m_running) {
        m_captureServer->setActive(true);
        m_running = true;
    }
    return true;
#else
    Q_UNUSED(exe);
    return false;
#endif
}

void OverlayController::saveKnownGames() const
{
#ifdef Q_OS_WIN
    const QString path = managedGamesFilePath();
    if (m_knownGames.isEmpty()) {
        QFile::remove(path);
        return;
    }

    QJsonArray arr;
    for (const QString &exe : m_knownGames) {
        QJsonObject o;
        o[QStringLiteral("exe")] = exe;
        arr.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("current")] = m_currentGame;
    root[QStringLiteral("known")] = arr;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
#endif
}

void OverlayController::loadKnownGames()
{
#ifdef Q_OS_WIN
    QFile f(managedGamesFilePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    const QJsonArray arr = doc.isArray() ? doc.array() : doc.object().value(QStringLiteral("known")).toArray();
    if (doc.isObject()) m_currentGame = doc.object().value(QStringLiteral("current")).toString();

    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const QString exe = o.value(QStringLiteral("exe")).toString();
        if (exe.isEmpty()) continue;
        m_knownGames.insert(exe);
    }

    if (!m_currentGame.isEmpty() && !m_knownGames.contains(m_currentGame))
        m_currentGame.clear();
#endif
}
