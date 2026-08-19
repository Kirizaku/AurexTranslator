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

#ifndef TTSENGINE_H
#define TTSENGINE_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>

class QDialog;
class QProcess;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

class TtsEngine : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Starting, Ready, Failed };
    Q_ENUM(State)

    enum class Mode { Managed, External };
    Q_ENUM(Mode)

    enum class Kind { Offline, Online, Custom };
    Q_ENUM(Kind)

    explicit TtsEngine(QObject *parent = nullptr);
    ~TtsEngine() override;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual Kind kind() const = 0;
    virtual QString summary() const = 0;
    virtual QStringList availableVoices() const = 0;
    virtual QString voiceLabel(const QString &voice) const { return voice; }

    virtual bool needsVoiceToStart() const { return true; }
    virtual bool settingsChangeNeedsRestart(const QJsonObject &before, const QJsonObject &after) const;

    virtual QString sampleText() const;

    virtual bool hasSettings() const { return false; }
    virtual QDialog *createSettingsDialog(QWidget *parent)
    {
        Q_UNUSED(parent)
        return nullptr;
    }

    virtual QJsonObject saveSettings() const;
    virtual void loadSettings(const QJsonObject &settings);

    static constexpr int kSlowest = 50;
    static constexpr int kNormalSpeed = 100;
    static constexpr int kFastest = 200;

    int speed() const { return m_speed; }
    void setSpeed(int percent) { m_speed = qBound(kSlowest, percent, kFastest); }

    State state() const { return m_state; }
    Mode mode() const { return m_mode; }
    QString voice() const { return m_voice; }
    QString baseUrl() const { return m_baseUrl; }
    bool useExternalServer() const { return m_useExternal; }

    void start();
    void stop();

    void setVoice(const QString &voice);
    void setUseExternalServer(bool external) { m_useExternal = external; }
    void setExternalUrl(const QString &url) { m_externalUrl = url; }
    QString externalUrl() const { return m_externalUrl; }

    void synthesize(const QString &text);
    void cancelSynthesis();
    bool synthesizing() const { return m_synthesisReply != nullptr; }

    static QString normalizeUrl(const QString &raw);

signals:
    void stateChanged(TtsEngine::State state);
    void logLine(const QString &line);
    void audioReady(const QByteArray &wav);
    void voicesAvailable(const QStringList &voices);
    void errorOccurred(const QString &error);

protected:
    virtual QStringList serverArguments(const QString &voice) const = 0;
    virtual QString serverWorkingDirectory() const = 0;
    virtual QString voiceFromInfo(const QJsonObject &info) const = 0;

    virtual bool voiceChangeNeedsRestart(const QString &current, const QString &next) const;
    virtual bool prepareManagedStart() { return true; }
    virtual bool wantsServerVoiceList() const;

    virtual QJsonObject synthesisPayload(const QString &text) const;
    virtual QStringList adoptVoiceList(const QJsonObject &voices);

    virtual bool looksLikeAudio(const QByteArray &data) const;
    bool extractServerScript(const QString &resource, const QString &target);
    static QString sampleForLanguage(const QString &code);

    void connectExternal(const QString &url, const QString &voice);
    void startManaged(const QString &voice);

    QString m_voice;
    QString m_externalUrl;
    int m_speed = kNormalSpeed;
    bool m_useExternal = false;
    QStringList m_serverVoices;

private:
    void logWarning(const QString &line) const;

    void requestVoiceList();

    void beginWaitingForServer(int attempts);
    void setState(State state);
    void pollReadiness();
    void sendSynthesis(const QString &text);
    void readProcessOutput();
    QString endpoint(const QString &path) const;
    static quint16 pickFreePort();

    State m_state = State::Stopped;
    Mode m_mode = Mode::Managed;
    QString m_baseUrl;
    quint16 m_port = 0;
    int m_maxReadyAttempts = 0;

    QProcess *m_process = nullptr;
    QString m_pendingLine;

    QTimer *m_readyTimer = nullptr;
    int m_readyAttempts = 0;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_readyReply = nullptr;
    QNetworkReply *m_synthesisReply = nullptr;
    QString m_queuedText;
};

#endif // TTSE