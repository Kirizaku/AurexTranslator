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

#ifndef PYTHONENV_H
#define PYTHONENV_H

#include <QObject>
#include <QStringList>

class QProcess;

#ifdef Q_OS_WIN
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
#endif

class PythonEnv : public QObject
{
    Q_OBJECT

public:
    struct Interpreter {
        QString program;
        QStringList launchArgs;
        QString version;

        bool isValid() const { return !program.isEmpty(); }
        QStringList argsFor(const QStringList &args) const { return launchArgs + args; }
    };

    explicit PythonEnv(QObject *parent = nullptr);
    ~PythonEnv() override;

    static constexpr int kMinMajor = 3;
    static constexpr int kMinMinor = 10;

    static QString rootDir();    // <config>/python
    static QString venvDir();    // <config>/python/venv
    static QString venvPython(); // interpreter inside the virtual environment
    static QString venvPip();    // pip inside the virtual environment
    static QString dataDir(const QString &name);

    void setPreferredInterpreter(const QString &path);
    QString preferredInterpreter() const { return m_preferred; }

    Interpreter detectSystemPython();
    Interpreter systemInterpreter() const { return m_system; }

    static bool venvReady();
    bool modulesAvailable(const QStringList &modules) const;
#ifdef Q_OS_WIN
    bool busy() const { return m_process != nullptr || m_installerReply != nullptr; }
#else
    bool busy() const { return m_process != nullptr; }
#endif

    void install(const QStringList &packages = {});
    void uninstall(const QStringList &packages);
    void abort();

#ifdef Q_OS_WIN
    // Our own Python, downloaded from python.org when the system has none
    static QString windowsPythonVersion() { return QStringLiteral("3.14.7"); }
    static QString windowsInstallerUrl();
    static QString runtimeDir();    // <config>/python/runtime, our private Python
    static QString runtimePython(); // interpreter inside it

    void installWindowsPython(bool systemWide);
#endif

signals:
    void logLine(const QString &line);
    void stepStarted(const QString &title);
    void progress(qint64 received, qint64 total);
    void finished(bool ok, const QString &error);

private:
    struct Step {
        QString title;
        QString program;
        QStringList args;
    };

    void runNextStep();
    void startProcess(const Step &step);
    void failWith(const QString &error);
    void readProcessOutput();

    static QString probeVersion(const Interpreter &interpreter);
    static bool versionIsSupported(const QString &version);

    QString m_preferred;
    Interpreter m_system;

    QList<Step> m_steps;
    int m_currentStep = -1;
    QProcess *m_process = nullptr;
    QString m_pendingLine;
    QString m_stepOutput;
    bool m_aborted = false;

#ifdef Q_OS_WIN
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_installerReply = nullptr;
    QFile *m_installerFile = nullptr;
#endif
};

#endif // PYTHONENV_H
