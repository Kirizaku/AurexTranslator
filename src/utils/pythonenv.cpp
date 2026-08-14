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

#include "pythonenv.h"
#include "config.h"

#include <QDir>
#include <QProcess>
#ifdef Q_OS_WIN
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QVersionNumber>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

void hideChildConsole(QProcess *process)
{
#ifdef Q_OS_LINUX
    Q_UNUSED(process)
#else
    process->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
}

QString stripAnsiEscapes(const QString &text)
{
    static const QRegularExpression escape(QStringLiteral("\x1B\\[[0-9;?]*[ -/]*[@-~]"));

    QString clean = text;
    return clean.remove(escape);
}

void prepareChildEnvironment(QProcess *process)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("PIP_DISABLE_PIP_VERSION_CHECK"), QStringLiteral("1"));
    process->setProcessEnvironment(env);
    process->setProcessChannelMode(QProcess::MergedChannels);
    hideChildConsole(process);
}

#ifdef Q_OS_WIN
void appendRegisteredPythons(QList<PythonEnv::Interpreter> &candidates)
{
    const QStringList roots = {
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Python\\PythonCore"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Python\\PythonCore"),
    };

    for (const QString &root : roots) {
        QSettings registry(root, QSettings::NativeFormat);
        QStringList versions = registry.childGroups();
        std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
            return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
        });

        for (const QString &version : std::as_const(versions)) {
            const QString dir =
                registry.value(version + QStringLiteral("/InstallPath/Default")).toString();
            if (dir.isEmpty())
                continue;

            const QString exe = QDir(dir).filePath(QStringLiteral("python.exe"));
            if (QFileInfo::exists(exe))
                candidates.append({exe, {}, {}});
        }
    }
}
#endif

QList<PythonEnv::Interpreter> interpreterCandidates()
{
    QList<PythonEnv::Interpreter> candidates;

#ifdef Q_OS_LINUX
    candidates.append({QStringLiteral("python3"), {}, {}});
    candidates.append({QStringLiteral("python"), {}, {}});

    for (int minor = 14; minor >= PythonEnv::kMinMinor; --minor)
        candidates.append({QStringLiteral("python3.%1").arg(minor), {}, {}});
#else
    if (QFileInfo::exists(PythonEnv::runtimePython()))
        candidates.append({PythonEnv::runtimePython(), {}, {}});

    candidates.append({QStringLiteral("py"), {QStringLiteral("-3")}, {}});
    candidates.append({QStringLiteral("python"), {}, {}});
    appendRegisteredPythons(candidates);
#endif

    return candidates;
}

QString moduleProbe(const QStringList &modules)
{
    QStringList quoted;
    for (const QString &module : modules)
        quoted << QStringLiteral("'%1'").arg(module);

    return QStringLiteral("import importlib.util as u, sys\n"
                          "ok = True\n"
                          "for m in [%1]:\n"
                          "    try:\n"
                          "        ok = ok and u.find_spec(m) is not None\n"
                          "    except Exception:\n"
                          "        ok = False\n"
                          "sys.exit(0 if ok else 1)\n")
        .arg(quoted.join(QLatin1String(", ")));
}

} // namespace

PythonEnv::PythonEnv(QObject *parent)
    : QObject(parent)
{
}

PythonEnv::~PythonEnv()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

QString PythonEnv::rootDir()
{
    return Config::getConfigDirPath() + QStringLiteral("python");
}

QString PythonEnv::venvDir()
{
    return rootDir() + QStringLiteral("/venv");
}

QString PythonEnv::dataDir(const QString &name)
{
    return rootDir() + QLatin1Char('/') + name;
}

QString PythonEnv::venvPython()
{
#ifdef Q_OS_LINUX
    return venvDir() + QStringLiteral("/bin/python");
#else
    return venvDir() + QStringLiteral("/Scripts/python.exe");
#endif
}

#ifdef Q_OS_WIN
QString PythonEnv::runtimeDir()
{
    return rootDir() + QStringLiteral("/runtime");
}

QString PythonEnv::runtimePython()
{
    return runtimeDir() + QStringLiteral("/python.exe");
}
#endif

QString PythonEnv::venvPip()
{
#ifdef Q_OS_LINUX
    return venvDir() + QStringLiteral("/bin/pip");
#else
    return venvDir() + QStringLiteral("/Scripts/pip.exe");
#endif
}

bool PythonEnv::venvReady()
{
    return QFileInfo::exists(venvPython()) && QFileInfo::exists(venvPip());
}

void PythonEnv::setPreferredInterpreter(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (m_preferred == trimmed)
        return;

    m_preferred = trimmed;
    m_system = Interpreter{};
}

QString PythonEnv::probeVersion(const Interpreter &interpreter)
{
    QProcess process;
    hideChildConsole(&process);
    process.start(interpreter.program,
                  interpreter.argsFor(
                      {QStringLiteral("-c"),
                       QStringLiteral("import sys; print('%d.%d.%d' % sys.version_info[:3])")}));

    if (!process.waitForStarted(3000))
        return {};
    if (!process.waitForFinished(5000)) {
        process.kill();
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {};

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

bool PythonEnv::versionIsSupported(const QString &version)
{
    const QStringList parts = version.split(QLatin1Char('.'));
    if (parts.size() < 2)
        return false;

    bool okMajor = false;
    bool okMinor = false;
    const int major = parts.at(0).toInt(&okMajor);
    const int minor = parts.at(1).toInt(&okMinor);
    if (!okMajor || !okMinor)
        return false;

    return major > kMinMajor || (major == kMinMajor && minor >= kMinMinor);
}

PythonEnv::Interpreter PythonEnv::detectSystemPython()
{
    m_system = Interpreter{};

    emit logLine(QStringLiteral("Looking for a Python interpreter..."));

    QList<Interpreter> candidates;
    if (!m_preferred.isEmpty())
        candidates.append({m_preferred, {}, {}});
    else
        candidates = interpreterCandidates();

    for (Interpreter candidate : candidates) {
        const QString version = probeVersion(candidate);

        if (version.isEmpty()) {
            if (!m_preferred.isEmpty())
                emit logLine(QStringLiteral("%1 is not a working Python interpreter.")
                                 .arg(candidate.program));
            continue;
        }

        if (!versionIsSupported(version)) {
            emit logLine(QStringLiteral("Skipping %1: version %2 is older than %3.%4")
                             .arg(candidate.program, version)
                             .arg(kMinMajor)
                             .arg(kMinMinor));
            continue;
        }

        candidate.version = version;
        m_system = candidate;
        emit logLine(QStringLiteral("Found Python %1 (%2)").arg(version, candidate.program));
        break;
    }

    return m_system;
}

bool PythonEnv::modulesAvailable(const QStringList &modules) const
{
    if (modules.isEmpty() || !venvReady())
        return false;

    QProcess process;
    hideChildConsole(&process);
    process.start(venvPython(), {QStringLiteral("-c"), moduleProbe(modules)});

    if (!process.waitForFinished(10000)) {
        process.kill();
        return false;
    }

    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void PythonEnv::install(const QStringList &packages)
{
    if (busy())
        return;

    m_aborted = false;
    m_steps.clear();
    m_currentStep = -1;

    if (!m_system.isValid())
        detectSystemPython();

    if (!m_system.isValid()) {
        emit finished(false, tr("No suitable Python interpreter found."));
        return;
    }

    QDir().mkpath(rootDir());

    if (!venvReady()) {
        m_steps.append({tr("Creating the virtual environment"),
                        m_system.program,
                        m_system.argsFor({QStringLiteral("-m"), QStringLiteral("venv"), venvDir()})});
    } else {
        emit logLine(QStringLiteral("Reusing the existing virtual environment: %1").arg(venvDir()));
    }

    m_steps.append({tr("Updating pip"),
                    venvPython(),
                    {QStringLiteral("-m"), QStringLiteral("pip"), QStringLiteral("install"),
                     QStringLiteral("--upgrade"), QStringLiteral("pip")}});

    if (!packages.isEmpty()) {
        m_steps.append({tr("Installing %1").arg(packages.join(QLatin1String(", "))),
                        venvPython(),
                        QStringList{QStringLiteral("-m"), QStringLiteral("pip"),
                                    QStringLiteral("install"), QStringLiteral("--upgrade")}
                            + packages});
    }

    runNextStep();
}

void PythonEnv::runNextStep()
{
    ++m_currentStep;

    if (m_currentStep >= m_steps.size()) {
        emit finished(true, {});
        return;
    }

    startProcess(m_steps.at(m_currentStep));
}

void PythonEnv::startProcess(const Step &step)
{
    m_stepOutput.clear();
    m_pendingLine.clear();

    emit stepStarted(step.title);
    emit logLine(QStringLiteral("$ %1 %2").arg(step.program, step.args.join(QLatin1Char(' '))));

    m_process = new QProcess(this);
    prepareChildEnvironment(m_process);

    connect(m_process, &QProcess::readyRead, this, &PythonEnv::readProcessOutput);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            emit logLine(QStringLiteral("Failed to start the process."));
    });

    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                readProcessOutput();
                if (!m_pendingLine.isEmpty()) {
                    emit logLine(m_pendingLine);
                    m_pendingLine.clear();
                }

                m_process->deleteLater();
                m_process = nullptr;

                if (m_aborted) {
                    emit finished(false, tr("Cancelled."));
                    return;
                }

                if (status != QProcess::NormalExit || exitCode != 0) {
                    failWith(tr("\"%1\" failed with code %2.")
                                 .arg(m_steps.at(m_currentStep).title)
                                 .arg(exitCode));
                    return;
                }

                runNextStep();
            });

    m_process->start(step.program, step.args);
}

void PythonEnv::failWith(const QString &error)
{
    QString message = error;

    if (m_stepOutput.contains(QStringLiteral("ensurepip"), Qt::CaseInsensitive)) {
        message += QLatin1Char('\n');
        message += tr("The system Python has no venv module. "
                      "Install it, for example: sudo apt install python3-venv");
    }

    emit finished(false, message);
}

void PythonEnv::readProcessOutput()
{
    if (!m_process)
        return;

    const QString chunk = stripAnsiEscapes(QString::fromUtf8(m_process->readAll()));
    if (chunk.isEmpty())
        return;

    m_stepOutput += chunk;
    m_pendingLine += chunk;
    m_pendingLine.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const int lastBreak = m_pendingLine.lastIndexOf(QLatin1Char('\n'));
    if (lastBreak < 0)
        return;

    const QStringList lines = m_pendingLine.left(lastBreak).split(QLatin1Char('\n'));
    m_pendingLine = m_pendingLine.mid(lastBreak + 1);

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            emit logLine(trimmed);
    }
}

void PythonEnv::abort()
{
    m_aborted = true;

#ifdef Q_OS_WIN
    if (m_installerReply) {
        m_installerReply->abort();
        return;
    }
#endif

    if (!m_process)
        return;

    emit logLine(QStringLiteral("Cancelling..."));
    m_process->terminate();
    if (!m_process->waitForFinished(3000))
        m_process->kill();
}

#ifdef Q_OS_WIN
QString PythonEnv::windowsInstallerUrl()
{
    const QString version = windowsPythonVersion();
    return QStringLiteral("https://www.python.org/ftp/python/%1/python-%1-amd64.exe").arg(version);
}
#endif

void PythonEnv::uninstall(const QStringList &packages)
{
    if (busy() || packages.isEmpty())
        return;

    m_aborted = false;
    m_steps.clear();
    m_currentStep = -1;

    if (!venvReady()) {
        emit finished(false, tr("The Python environment is not there to remove anything from."));
        return;
    }

    m_steps.append({tr("Removing %1").arg(packages.join(QLatin1String(", "))),
                    venvPython(),
                    QStringList{QStringLiteral("-m"), QStringLiteral("pip"),
                                QStringLiteral("uninstall"), QStringLiteral("--yes")}
                        + packages});

    runNextStep();
}

#ifdef Q_OS_WIN
void PythonEnv::installWindowsPython(bool systemWide)
{
    if (busy())
        return;

    m_aborted = false;

    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    const QString path = QDir::temp().filePath(
        QStringLiteral("python-%1-amd64.exe").arg(windowsPythonVersion()));

    m_installerFile = new QFile(path, this);
    if (!m_installerFile->open(QIODevice::WriteOnly)) {
        delete m_installerFile;
        m_installerFile = nullptr;
        emit finished(false, tr("Cannot write to %1").arg(path));
        return;
    }

    emit stepStarted(tr("Downloading Python %1").arg(windowsPythonVersion()));
    emit logLine(windowsInstallerUrl());

    QNetworkRequest request{QUrl(windowsInstallerUrl())};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_installerReply = m_network->get(request);

    connect(m_installerReply, &QNetworkReply::downloadProgress, this, &PythonEnv::progress);
    connect(m_installerReply, &QNetworkReply::readyRead, this, [this] {
        m_installerFile->write(m_installerReply->readAll());
    });

    connect(m_installerReply, &QNetworkReply::finished, this, [this, systemWide] {
        const bool ok = m_installerReply->error() == QNetworkReply::NoError;
        const QString error = m_installerReply->errorString();

        m_installerFile->write(m_installerReply->readAll());
        const QString path = m_installerFile->fileName();
        m_installerFile->close();

        m_installerReply->deleteLater();
        m_installerReply = nullptr;
        m_installerFile->deleteLater();
        m_installerFile = nullptr;

        if (!ok) {
            QFile::remove(path);
            emit finished(false, m_aborted ? tr("Cancelled.") : error);
            return;
        }

        emit logLine(QStringLiteral("A user account control prompt may appear."));

        QStringList args{ QStringLiteral("/passive"),
                         QStringLiteral("InstallAllUsers=0"),
                         QStringLiteral("Include_pip=1"),
                         QStringLiteral("Include_test=0") };

        if (systemWide) {
            args << QStringLiteral("PrependPath=1");
        }
        else {
            args << QStringLiteral("PrependPath=0")
                << QStringLiteral("Include_launcher=0")
                << QStringLiteral("AssociateFiles=0")
                << QStringLiteral("Shortcuts=0")
                << QStringLiteral("TargetDir=") + QDir::toNativeSeparators(runtimeDir());
        }

        m_steps.clear();
        m_currentStep = -1;
        m_steps.append({ tr("Running the installer"), path, args });

        runNextStep();
        });
}
#endif
