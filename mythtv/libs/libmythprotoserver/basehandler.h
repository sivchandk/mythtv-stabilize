
#include <QString>
#include <QStringList>

#include "handler.h"
#include "mythsocket.h"

class MainServer;

class BaseProtoHandler : public ProtoHandler
{
  public:
    QString getType(void)         const { return QString("BASE"); }

    bool HandleAnnounce(MainServer *ms, MythSocket *socket,
                        QStringList &commands, QStringList &slist);
    bool HandleCommand(MainServer *ms, MythSocket *socket,
                        QStringList &commands, QStringList &slist);
  private:
    bool HandleQueryLoad(ProtoSocketHandler *sock);
    bool HandleQueryUptime(ProtoSocketHandler *sock);
    bool HandleQueryHostname(ProtoSocketHandler *sock);
    bool HandleQueryMemStats(ProtoSocketHandler *sock);
    bool HandleQueryTimeZone(ProtoSocketHandler *sock);
};

class BaseSocketHandler : public ProtoSocketHandler
{
  public:
    BaseSocketHandler(MainServer *parent, MythSocket *sock,
                        QString hostname);
    BaseSocketHandler(BaseSocketHandler &other);
                
    QString getType(void)         const { return QString("BASE"); }
    void Shutdown(void);
  private:

};
