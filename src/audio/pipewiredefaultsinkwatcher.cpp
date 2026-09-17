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

#include "pipewiredefaultsinkwatcher.h"
#include "src/utils/logger.h"

#include <QSocketNotifier>
#include <spa/utils/json.h>

PipeWireDefaultSinkWatcher::PipeWireDefaultSinkWatcher(QObject *parent)
    : QObject{parent}
{
    pw_init(nullptr, nullptr);

    loop = pw_main_loop_new(nullptr);
    context = pw_context_new(pw_main_loop_get_loop(loop), nullptr, 0);

    if (!connectCore()) {
        Log(Logger::Level::Warning, "[pipewire] Default sink watcher couldn't connect to PipeWire");
        return;
    }

    const int fd = pw_loop_get_fd(pw_main_loop_get_loop(loop));
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        pw_loop *l = pw_main_loop_get_loop(loop);
        pw_loop_enter(l);
        pw_loop_iterate(l, 0);
        pw_loop_leave(l);
    });
}

PipeWireDefaultSinkWatcher::~PipeWireDefaultSinkWatcher()
{
    delete m_notifier;

    disconnectCore();

    if (context) {
        pw_context_destroy(context);
        context = nullptr;
    }

    if (loop) {
        pw_main_loop_destroy(loop);
        loop = nullptr;
    }
}

bool PipeWireDefaultSinkWatcher::connectCore()
{
    core = pw_context_connect(context, nullptr, 0);
    if (!core)
        return false;

    registry =pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(registry, &registry_listener, &registry_events, this);
    return true;
}

void PipeWireDefaultSinkWatcher::disconnectCore()
{
    if (!core)
        return;

    if (metadata) {
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(metadata));
        metadata = nullptr;
    }

    if (registry) {
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
        registry = nullptr;
    }

    pw_core_disconnect(core);
    core = nullptr;
}

void PipeWireDefaultSinkWatcher::reevaluate()
{
    const QString description = m_sinkDescriptions.value(m_defaultSinkName);
    if (description.isEmpty() || description == m_currentDescription)
        return;

    m_currentDescription = description;
    emit defaultSinkChanged(description);
}

void PipeWireDefaultSinkWatcher::on_registry_global(void* userdata, uint32_t id, uint32_t permissions,
                                                    const char* type, uint32_t version, const spa_dict* props)
{
    Q_UNUSED(permissions)
    Q_UNUSED(version)

    auto self = static_cast<PipeWireDefaultSinkWatcher*>(userdata);
    if (props == nullptr)
        return;

    if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
        const char* name = spa_dict_lookup(props, "metadata.name");
        if (name == nullptr || std::strcmp(name, "default") != 0 || self->metadata)
            return;

        self->metadata = static_cast<pw_metadata*>(pw_registry_bind(self->registry, id, type, PW_VERSION_METADATA, 0));

        if (self->metadata)
            pw_metadata_add_listener(self->metadata, &self->metadata_listener, &metadata_events, self);

        return;
    }

    if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char* mediaClass = spa_dict_lookup(props, "media.class");
        const char* name = spa_dict_lookup(props, "node.name");
        if (mediaClass == nullptr || name == nullptr || std::strstr(mediaClass, "Audio/Sink") == nullptr)
            return;

        const char* description = spa_dict_lookup(props, "node.description");
        self->m_sinkNames.insert(id, QString::fromUtf8(name));
        self->m_sinkDescriptions.insert(QString::fromUtf8(name), QString::fromUtf8(description ? description : name));
        self->reevaluate();
    }
}

void PipeWireDefaultSinkWatcher::on_registry_global_remove(void* userdata, uint32_t id)
{
    auto self = static_cast<PipeWireDefaultSinkWatcher*>(userdata);

    const QString name = self->m_sinkNames.take(id);
    if (!name.isEmpty())
        self->m_sinkDescriptions.remove(name);
}

int PipeWireDefaultSinkWatcher::on_metadata_property(void* userdata, uint32_t subject, const char* key,
                                                     const char* type, const char* value)
{
    Q_UNUSED(subject)
    Q_UNUSED(type)

    auto self = static_cast<PipeWireDefaultSinkWatcher*>(userdata);
    if (key == nullptr || value == nullptr || std::strcmp(key, "default.audio.sink") != 0)
        return 0;

    spa_json root;
    spa_json_init(&root, value, std::strlen(value));
    spa_json object;
    if (spa_json_enter_object(&root, &object) <= 0)
        return 0;

    char propertyKey[64];
    char propertyValue[256];
    while (spa_json_get_string(&object, propertyKey, sizeof(propertyKey)) > 0) {
        if (std::strcmp(propertyKey, "name") == 0 && spa_json_get_string(&object, propertyValue, sizeof(propertyValue)) > 0)
            self->m_defaultSinkName = QString::fromUtf8(propertyValue);
        else
            spa_json_next(&object, nullptr);
    }

    self->reevaluate();
    return 0;
}

const pw_registry_events PipeWireDefaultSinkWatcher::registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

const pw_metadata_events PipeWireDefaultSinkWatcher::metadata_events = {
    .version = PW_VERSION_METADATA_EVENTS,
    .property = on_metadata_property,
};
