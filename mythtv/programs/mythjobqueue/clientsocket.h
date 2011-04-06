#ifndef CLIENTSOCKETHANDLER_H_
#define CLIENTSOCKETHANDLER_H_

#include <QString>
#include <QStringList>

#include "mythsocketmanager.h"
#include "mythsocket.h"

class ClientSocketHandler : public SocketRequestHandler
{
  public:
    bool HandleAnnounce(MythSocket *socket, QStringList &commands,
                                QStringList &slist);
    bool HandleQuery(MythSocket *socket, QStringList &commands,
                                QStringList &slist);
    QString GetHandlerName(void)            { return "BASE"; }

  private:
    bool HandleSetVerbose(MythSocket *socket, QStringList &slist);
    bool HandleQueryLoad(MythSocket *socket);
    bool HandleQueryUptime(MythSocket *socket);
    bool HandleQueryHostname(MythSocket *socket);
    bool HandleQueryMemStats(MythSocket *socket);
    bool HandleQueryTimeZone(MythSocket *socket);
};

#endif
