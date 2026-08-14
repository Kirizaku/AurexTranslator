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

#ifndef PYTHONCONTROLLER_H
#define PYTHONCONTROLLER_H

#include "src/utils/pythonenv.h"

#include <QObject>
#include <QStringList>

class ProcessLogWindow;
class QWidget;

class PythonController : public QObject
{
    Q_OBJECT

public:
    struct Component {
        QString id;
        QString name;
        QStringList packages;
        QStringList modules;
        QString note;
        QStringList installArgs;
    };

    using ConfirmHandler = std::function<bool(const Component &)>;
    void setConfirmHandler(ConfirmHandler handler) { m_confirm = std::move(handler); }

    explicit PythonController(QWidget *parent = nullptr);

    void registerComponent(const Component &component);
    void setComponentInstall(const QString &id, const QStringList &packages,
                             const QStringList &installArgs);
    QList<Component> components() const { return m_components; }

    bool interpreterReady() const { return m_env->systemInterpreter().isValid(); }
    static bool environmentReady() { return PythonEnv::venvReady(); }
    bool componentReady(const QString &id) const;

    QString interpreterDescription() const;

    bool busy() const { return m_env->busy(); }
    void detect();
    void ensureDetected();
    void setPreferredInterpreter(const QString &path);
    void setupEnvironment();

    void installComponent(const QString &id, bool force = false);
    QStringList removablePackages(const QString &id) const;
    void uninstallComponent(const QString &id);

#ifdef Q_OS_WIN
    void installPython(bool systemWide);
#endif

    const Component *component(const QString &id) const;

    void showLog();

signals:
    void statusChanged();
    void jobFinished(const QString &id, bool ok, const QString &error);

private:
    void beginJob(const QString &title, const QString &componentId);

    PythonEnv *m_env = nullptr;
    ProcessLogWindow *m_logWindow = nullptr;

    ConfirmHandler m_confirm;
    QList<Component> m_components;
    QString m_currentComponent;
    bool m_detected = false;
    bool m_interpreterJob = false;
};

#endif // PYTHONCONTROLLER_H
