#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include <QtPlugin>
#include <QString>

class PluginInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~PluginInterface() = default;

    virtual void setLanguage(const QString &languageCode) = 0;
    virtual QString execute(const QString &command, const QStringList &args = QStringList()) = 0;

signals:
    void pluginMessage(const QString &msg);
    void currentOutput(const QString &source, const QString &out);
    void processLost();
};

#define PluginInterface_iid "com.aurextranslator.PluginInterface"
Q_DECLARE_INTERFACE(PluginInterface, PluginInterface_iid)

#endif // PLUGININTERFACE_H
