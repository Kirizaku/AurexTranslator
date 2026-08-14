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

#include "pythoncontroller.h"

#include "src/UI/processlogwindow.h"
#include "src/utils/logger.h"

#include <QWidget>
#include <QRegularExpression>

PythonController::PythonController(QWidget *parent)
    : QObject(parent)
    , m_env(new PythonEnv(this))
    , m_logWindow(new ProcessLogWindow(parent))
{
    m_logWindow->hide();

    connect(m_env, &PythonEnv::logLine, this, [this](const QString &line) {
        m_logWindow->appendLine(line);
        Log(Logger::Level::Info, QStringLiteral("[python] ") + line);
    });
    connect(m_env, &PythonEnv::stepStarted, m_logWindow, &ProcessLogWindow::setStep);
    connect(m_env, &PythonEnv::progress, m_logWindow, &ProcessLogWindow::setProgress);
    connect(m_logWindow, &ProcessLogWindow::abortRequested, m_env, &PythonEnv::abort);

    connect(m_env, &PythonEnv::finished, this, [this](bool ok, const QString &error) {
        m_logWindow->endJob(ok, error);

        if (!ok)
            Log(Logger::Level::Warning, QStringLiteral("[python] ") + error);

        const QString component = m_currentComponent;
        m_currentComponent.clear();

        if (ok && m_interpreterJob)
            detect();
        m_interpreterJob = false;

        emit jobFinished(component, ok, error);
        emit statusChanged();
    });
}

void PythonController::registerComponent(const Component &component)
{
    for (const Component &known : std::as_const(m_components)) {
        if (known.id == component.id)
            return;
    }

    m_components.append(component);
    emit statusChanged();
}

void PythonController::setComponentInstall(const QString &id, const QStringList &packages,
                                           const QStringList &installArgs)
{
    for (Component &component : m_components) {
        if (component.id != id)
            continue;

        component.packages = packages;
        component.installArgs = installArgs;
        emit statusChanged();
        return;
    }
}

bool PythonController::componentReady(const QString &id) const
{
    const Component *found = component(id);
    return found && m_env->modulesAvailable(found->modules);
}

QString PythonController::interpreterDescription() const
{
    const PythonEnv::Interpreter interpreter = m_env->systemInterpreter();
    if (!interpreter.isValid())
        return {};

    return QStringLiteral("Python %1 (%2)").arg(interpreter.version, interpreter.program);
}

void PythonController::detect()
{
    m_detected = true;
    m_env->detectSystemPython();
    emit statusChanged();
}

void PythonController::ensureDetected()
{
    if (!m_detected)
        detect();
}

void PythonController::setPreferredInterpreter(const QString &path)
{
    if (path.trimmed() == m_env->preferredInterpreter())
        return;

    m_env->setPreferredInterpreter(path);
    m_detected = false;
}

void PythonController::beginJob(const QString &title, const QString &componentId)
{
    m_currentComponent = componentId;
    m_logWindow->beginJob(title);
}

void PythonController::setupEnvironment()
{
    if (busy())
        return;

    beginJob(tr("Preparing the Python environment"), {});
    m_env->install();
}

void PythonController::installComponent(const QString &id, bool force)
{
    if (busy())
        return;

    const Component *found = component(id);

    if (!found) {
        Log(Logger::Level::Warning, QStringLiteral("[python] Unknown component: ") + id);
        emit jobFinished(id, false, tr("Unknown component: %1").arg(id));
        return;
    }

    if (!force && m_env->modulesAvailable(found->modules)) {
        emit jobFinished(id, true, {});
        return;
    }

    if (m_confirm && !m_confirm(*found)) {
        emit jobFinished(id, false, {});
        return;
    }

    beginJob(tr("Installing %1").arg(found->name), id);
    m_env->install(found->packages + found->installArgs);
}

namespace {

QString normalized(const QString &package)
{
    QString name = package.section(QLatin1Char('['), 0, 0).trimmed().toLower();

    static const QRegularExpression separators(QStringLiteral("[-_.]+"));
    return name.replace(separators, QStringLiteral("-"));
}

} // namespace

QStringList PythonController::removablePackages(const QString &id) const
{
    const Component *found = component(id);
    if (!found)
        return {};

    QSet<QString> keep;
    for (const Component &other : std::as_const(m_components)) {
        if (other.id == id || !componentReady(other.id))
            continue;

        for (const QString &package : other.packages)
            keep.insert(normalized(package));
        for (const QString &module : other.modules)
            keep.insert(normalized(module));
    }

    QStringList removable;
    for (const QString &package : found->packages) {
        if (!keep.contains(normalized(package)))
            removable.append(normalized(package));
    }

    return removable;
}

void PythonController::uninstallComponent(const QString &id)
{
    if (busy())
        return;

    const Component *found = component(id);
    if (!found) {
        Log(Logger::Level::Warning, QStringLiteral("[python] Unknown component: ") + id);
        emit jobFinished(id, false, tr("Unknown component: %1").arg(id));
        return;
    }

    const QStringList packages = removablePackages(id);

    if (packages.isEmpty()) {
        emit jobFinished(id, true, {});
        emit statusChanged();
        return;
    }

    beginJob(tr("Removing %1").arg(found->name), id);
    m_env->uninstall(packages);
}

#ifdef Q_OS_WIN
void PythonController::installPython(bool systemWide)
{
    if (busy())
        return;

    m_interpreterJob = true;
    beginJob(tr("Installing Python %1").arg(PythonEnv::windowsPythonVersion()), {});
    m_env->installWindowsPython(systemWide);
}
#endif

const PythonController::Component *PythonController::component(const QString &id) const
{
    for (const Component &component : std::as_const(m_components)) {
        if (component.id == id)
            return &component;
    }
    return nullptr;
}

void PythonController::showLog()
{
    m_logWindow->show();
    m_logWindow->raise();
    m_logWindow->activateWindow();
}
