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

#ifndef PIPEWIREDEFAULTSINKWATCHER_H
#define PIPEWIREDEFAULTSINKWATCHER_H

#include <QObject>
#include <QHash>
#include <QSocketNotifier>
#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>

class PipeWireDefaultSinkWatcher : public QObject
{
    Q_OBJECT
public:
    explicit PipeWireDefaultSinkWatcher(QObject *parent = nullptr);
    ~PipeWireDefaultSinkWatcher();

    QString currentDescription() const { return m_currentDescription; }

signals:
    void defaultSinkChanged(const QString &description);

private:
    pw_main_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;
    pw_metadata* metadata = nullptr;

    spa_hook registry_listener{};
    spa_hook metadata_listener{};

    bool connectCore();
    void disconnectCore();
    void reevaluate();

    static void on_registry_global(void* userdata, uint32_t id, uint32_t permissions,
                                   const char* type, uint32_t version, const spa_dict* props);
    static void on_registry_global_remove(void* userdata, uint32_t id);
    static int on_metadata_property(void* userdata, uint32_t subject, const char* key,
                                    const char* type, const char* value);

    static const pw_registry_events registry_events;
    static const pw_metadata_events metadata_events;

    QSocketNotifier *m_notifier = nullptr;

    QString m_defaultSinkName;
    QString m_currentDescription;
    QHash<QString, QString> m_sinkDescriptions;
    QHash<uint32_t, QString> m_sinkNames;
};

#endif // PIPEWIREDEFAULTSINKWATCHER_H
