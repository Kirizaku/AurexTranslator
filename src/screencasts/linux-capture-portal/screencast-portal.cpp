#include "screencast-portal.h"
#include "src/utils/logger.h"

Q_DECLARE_METATYPE(ScreenCastPortal::Stream);
Q_DECLARE_METATYPE(ScreenCastPortal::Streams);

ScreenCastPortal::ScreenCastPortal(QString setCurrentRestoreToken, QObject *parent)
    : QObject{parent}
{
    m_restoreToken = setCurrentRestoreToken;
}

const QDBusArgument &operator >> (const QDBusArgument &arg, ScreenCastPortal::Stream &stream)
{
    arg.beginStructure();
    arg >> stream.node_id;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant map;
        arg.beginMapEntry();
        arg >> key >> map;
        arg.endMapEntry();
        stream.map.insert(key, map);
    }
    arg.endMap();
    arg.endStructure();

    return arg;
}

QString ScreenCastPortal::getSessionToken()
{
    m_sessionTokenCounter += 1;
    return QString("u%1").arg(m_sessionTokenCounter);
}

QString ScreenCastPortal::getRequestToken()
{
    m_requestTokenCounter += 1;
    return QString("u%1").arg(m_requestTokenCounter);
}

void ScreenCastPortal::reload()
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop",
                                                          m_screenCastSession.path(),
                                                          QLatin1String("org.freedesktop.portal.Session"),
                                                          QLatin1String("Close"));
    QDBusPendingCall reply = QDBusConnection::sessionBus().asyncCall(message);

    m_restoreToken = "";
    init();
}

void ScreenCastPortal::init()
{
    m_screencast = new OrgFreedesktopPortalScreenCastInterface(QLatin1String("org.freedesktop.portal.Desktop"),
                                                               QLatin1String("/org/freedesktop/portal/desktop"),
                                                               QDBusConnection::sessionBus(), this);
    auto reply = m_screencast->CreateSession({
                                              { QLatin1String("session_handle_token"), getSessionToken() },
                                              { QLatin1String("handle_token"), getRequestToken() },
                                              });
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[pipewire] Couldn't get reply: %1").arg(reply.error().message()));
    } else {
        QDBusConnection::sessionBus().connect(QString(),
                                            reply.value().path(),
                                            QLatin1String("org.freedesktop.portal.Request"),
                                            QLatin1String("Response"),
                                            this,
                                            SLOT(gotCreateSessionResponse(uint,QVariantMap)));
    }
}

void ScreenCastPortal::gotCreateSessionResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        Log(Logger::Level::Warning, QString("[pipewire] Failed to create session: %1").arg(response));
        emit failedPortal();
        return;
    }

    Log(Logger::Level::Info, "[pipewire] Screencast session created");

    m_screenCastSession = QDBusObjectPath(results["session_handle"].toString());

    QVariantMap options;
    options[QLatin1String("handle_token")] = getRequestToken();
    options[QLatin1String("multiple")] = false;
    options[QLatin1String("types")] = (uint)3;
    if (m_restoreToken != "") {
        options[QLatin1String("restore_token")] = m_restoreToken;
    }
    options[QLatin1String("persist_mode")] = (uint)2;

    auto reply = m_screencast->SelectSources(m_screenCastSession, options);

    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[pipewire] Failed to call ListShortcuts: %1").arg(reply.error().message()));
        return;
    }

    QDBusConnection::sessionBus().connect(QString(),
                                          reply.value().path(),
                                          QLatin1String("org.freedesktop.portal.Request"),
                                          QLatin1String("Response"),
                                          this,
                                          SLOT(gotSelectSourcesResponse(uint,QVariantMap)));
}

void ScreenCastPortal::gotSelectSourcesResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        Log(Logger::Level::Warning, QString("[pipewire] Failed to select sources: %1").arg(response));
        return;
    }

    auto reply = m_screencast->Start(m_screenCastSession, "",
                                     {
                                         {QLatin1String("handle_token"), getRequestToken() }
                                     });
    reply.waitForFinished();
    if (reply.isError()) {
        Log(Logger::Level::Warning, QString("[pipewire] Failed to call ListShortcuts: %1").arg(reply.error().message()));
        return;
    }

    QDBusConnection::sessionBus().connect(QString(),
                                          reply.value().path(),
                                          QLatin1String("org.freedesktop.portal.Request"),
                                          QLatin1String("Response"),
                                          this,
                                          SLOT(gotStartResponse(uint,QVariantMap)));
}

void ScreenCastPortal::gotStartResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        Log(Logger::Level::Warning, QString("[pipewire] Failed to start: %1").arg(response));
    }

    if (results.contains(QLatin1String("restore_token"))) {
        emit currentRestoreToken(results.value(QLatin1String("restore_token")).toString());
    }

    Streams streams = qdbus_cast<Streams>(results.value(QLatin1String("streams")));
    for (const auto &stream : streams) {
        auto reply = m_screencast->OpenPipeWireRemote(m_screenCastSession,
                                                      {});
        reply.waitForFinished();
        if (reply.isError()) {
            Log(Logger::Level::Warning, QString("[pipewire] Failed to call OpenPipeWireRemote: %1").arg(reply.error().message()));
            return;
        }
        emit currentNodeId(stream.node_id);
    }
}
