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

#ifndef OVERLAYCONTROLLER_H
#define OVERLAYCONTROLLER_H

#include <QObject>
#include <QThread>

#include "src/utils/pluginloader.h"

class PluginInterface;
class OverlayCaptureServer;

class OverlayController : public QObject
{
    Q_OBJECT

public:
    explicit OverlayController(QObject *parent = nullptr);
    ~OverlayController() override;

    void setPlugin(PluginInterface *plugin);
    bool isPluginLoaded() const { return m_injectorPlugin != nullptr; }
    void setRegistry(const QList<PluginManager::PluginInfo> &registry);
    bool isRunning() const { return m_running; }
    void replayGameState();

    QString installVulkanSupport(bool *ok = nullptr);
    QString uninstallVulkanSupport();
    bool isVulkanLayerRegistered() const;
    QString launcherPath() const;

    void setFrameCaptureAllowed(bool allowed);
    void setShowOriginal(bool show);
    void setShowSource(bool show);
    void setShowTranslatorName(bool show);

    void setStyle(const QColor &textColor, const QColor &bgColor, float fontSize,
                  float zoneX, float zoneY, float zoneW, float zoneH, bool fontSizeAuto,
                  int cornerRadius, Qt::Alignment align,
                  int marginTop, int marginBottom, int marginLeft, int marginRight);

    void setFontPath(const QString &path);
    void restorePersistedGames();
    void ensureKnownGamesLoaded();

public slots:
    void addGame(const QString &exe);
    void removeGame(const QString &exe);
    void switchToGame(const QString &exe);
    void unloadAllGames();

    void start();
    void stop();

    void setPreviewActive(bool active);
    void setActiveGame(const QString &exe, quint32 pid = 0);
    void setTextDisplayEnabled(bool enabled);
    void setHiddenByHotkey(bool hidden);
    bool isHiddenByHotkey() const { return m_hiddenByHotkey; }
    void setTranslationResult(const QString &source, const QString &translatorName, const QString &original, const QString &result);
    void setOriginalText(const QString &source, const QString &original, bool translationPending);
    void clearResultsBySource(const QString &source);
    void clearResultsByTranslator(const QString &translatorName);

signals:
    void infoMessage(const QString &message);
    void hiddenChanged(bool hidden);
    void sourceDisconnected();
    void gameListChanged(const QStringList &exeNames, const QList<quint32> &pids);
    void activeGameChanged(const QString &exe, quint32 pid);
    void managedGamesChanged(const QStringList &searchingGames,
                             const QStringList &injectingGames);
    void knownGamesChanged(const QStringList &otherGames);

private:
    struct ManagedGame {
        bool processFound = false;
    };

    static const QString kPluginName;

    void announceActiveGame(const QString &exe, quint32 pid);

    void pushText(const QString &text);
    void recomposeAndPush();
    QString composeText() const;
    void emitManagedGamesChanged();
    void emitKnownGamesChanged();
    void onGameProcessFound(const QString &processName);
    void onGameProcessLost(const QString &processName);
    void stopGameSession(const QString &exe);
    bool startManaging(const QString &exe);
    void saveKnownGames() const;
    void loadKnownGames();

    PluginInterface *m_injectorPlugin = nullptr;
    QList<PluginManager::PluginInfo> m_registry;
    QMap<QString, ManagedGame> m_managedGames;
    QSet<QString> m_knownGames;
    QString m_currentGame;
    bool m_knownGamesLoaded = false;
    bool m_running = false;

    QStringList m_gameNames;
    QList<quint32> m_gamePids;
    QString m_activeGameExe;
    quint32 m_activeGamePid = 0;

    QThread m_captureThread;
    OverlayCaptureServer *m_captureServer = nullptr;

    struct TranslationEntry {
        QString source;
        QString translatorName;
        QString original;
        QString result;
    };
    QList<TranslationEntry> m_entries;
    bool m_showOriginal = false;
    bool m_showSource = false;
    bool m_showTranslatorName = false;

    bool m_previewActive = false;
    bool m_textDisplayEnabled = false;
    bool m_hiddenByHotkey = false;
};

#endif // OVERLAYCONTROLLER_H
