#ifndef XDG_PORTAL_H
#define XDG_PORTAL_H

#include <QObject>
#include <QImage>
#include <QTimer>
#include <screencast_portal_interface.h>

class OrgFreedesktopPortalScreenCastInterface;

class ScreenCastPortal : public QObject
{
    Q_OBJECT
public:
    struct Stream {
        uint node_id;
        QVariantMap map;
    };
    using Streams = QList<Stream>;

    explicit ScreenCastPortal(QString setCurrentRestoreToken, QObject *parent = nullptr);
    void init();
    void reload();

public Q_SLOTS:
    void gotCreateSessionResponse(uint response, const QVariantMap &results);
    void gotSelectSourcesResponse(uint response, const QVariantMap &results);
    void gotStartResponse(uint response, const QVariantMap &results);

signals:
    void currentNodeId(uint node_id);
    void currentRestoreToken(QString restore_token);
    void failedPortal();

private:
    OrgFreedesktopPortalScreenCastInterface *m_screencast;
    uint m_sessionTokenCounter = 0;
    uint m_requestTokenCounter = 0;
    QDBusObjectPath m_screenCastSession;
    QString m_restoreToken;

    QString getSessionToken();
    QString getRequestToken();
};

#endif // XDG_PORTAL_H
