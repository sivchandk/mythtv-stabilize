
#include <QString>
#include <QStringList>

#include "mythsystemevent.h"
#include "mainserver.h"
#include "handler.h"
#include "basehandler.h"
#include "mythsocket.h"
#include "util.h"
#include "mythverbose.h"
#include "mythcorecontext.h"


bool BaseProtoHandler::HandleAnnounce(MainServer *ms, MythSocket *socket,
                                QStringList &commands, QStringList &slist)
{
    VERBOSE(VB_IMPORTANT, QString("Attempting handler: %1").arg(getType()));
    if (commands.size() != 4)
        return false;

    bool blockShutdown;
    if (commands[1] == "Playback")
        blockShutdown = true;
    else if (commands[1] == "Monitor")
        blockShutdown = false;
    else
        return false;

    QString hostname = commands[2];
    ProtoSockEventsMode eventsMode = (ProtoSockEventsMode)commands[3].toInt();

    ProtoSocketHandler *sock = ms->GetHandlerSock(socket, "BASE");
    // announce already handled
    if (sock != NULL)
    {
        QStringList sl; sl << "OK";
        sock->SendStringList(sl);
        sock->SetBlockShutdown(blockShutdown);
        sock->SetEventsMode(eventsMode);
        return true;
    }

    sock = new BaseSocketHandler(ms, socket, hostname);
    sock->SetEventsMode(eventsMode);
    ms->AddHandlerSock(sock);

    QStringList sl; sl << "OK";
    sock->SendStringList(sl);
    sock->DownRef();

    VERBOSE(VB_GENERAL, QString("MainServer::ANN %1")
                                    .arg(commands[1]));
    VERBOSE(VB_IMPORTANT, QString("adding: %1 as a client (events: %2)")
                               .arg(commands[2]).arg(eventsMode));
    SendMythSystemEvent(QString("CLIENT_CONNECTED HOSTNAME %1")
                                    .arg(commands[2]));

    return true;
}

bool BaseProtoHandler::HandleCommand(MainServer *ms, MythSocket *socket,
                                QStringList &commands, QStringList &slist)
{
    QString command = commands[0];
    bool res = false;

    ProtoSocketHandler *sock = ms->GetHandlerSock(socket, "BASE");
    if (sock == NULL)
        return res;

    if (command == "QUERY_LOAD")
        res = HandleQueryLoad(sock);
    else if (command == "QUERY_UPTIME")
        res = HandleQueryUptime(sock);
    else if (command == "QUERY_HOSTNAME")
        res = HandleQueryHostname(sock);
    else if (command == "QUERY_MEMSTATS")
        res = HandleQueryMemStats(sock);
    else if (command == "QUERY_TIME_ZONE")
        res = HandleQueryTimeZone(sock);

    return res;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_LOAD
 * Returns the Unix load on this backend
 * (three floats - the average over 1, 5 and 15 mins).
 */
bool BaseProtoHandler::HandleQueryLoad(ProtoSocketHandler *sock)
{
    QStringList strlist;

    double loads[3];
    if (getloadavg(loads,3) == -1)
    {
        strlist << "ERROR";
        strlist << "getloadavg() failed";
    }
    else
        strlist << QString::number(loads[0])
                << QString::number(loads[1])
                << QString::number(loads[2]);

    sock->SendStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_UPTIME
 * Returns the number of seconds this backend's host has been running
 */
bool BaseProtoHandler::HandleQueryUptime(ProtoSocketHandler *sock)
{
    QStringList strlist;
    time_t      uptime;

    if (getUptime(uptime))
        strlist << QString::number(uptime);
    else
    {
        strlist << "ERROR";
        strlist << "Could not determine uptime.";
    }

    sock->SendStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_HOSTNAME
 * Returns the hostname of this backend
 */
bool BaseProtoHandler::HandleQueryHostname(ProtoSocketHandler *sock)
{
    QStringList strlist;

    strlist << gCoreContext->GetHostName();

    sock->SendStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_MEMSTATS
 * Returns total RAM, free RAM, total VM and free VM (all in MB)
 */
bool BaseProtoHandler::HandleQueryMemStats(ProtoSocketHandler *sock)
{
    QStringList strlist;
    int         totalMB, freeMB, totalVM, freeVM;

    if (getMemStats(totalMB, freeMB, totalVM, freeVM))
    {
        strlist << QString::number(totalMB) << QString::number(freeMB)
                << QString::number(totalVM) << QString::number(freeVM);
    }
    else
    {
        strlist << "ERROR"
                << "Could not determine memory stats.";
    }

    sock->SendStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_TIME_ZONE
 * Returns time zone ID, current offset, current time
 */
bool BaseProtoHandler::HandleQueryTimeZone(ProtoSocketHandler *sock)
{
    QStringList strlist;
    strlist << getTimeZoneID() << QString::number(calc_utc_offset())
            << mythCurrentDateTime().toString(Qt::ISODate);

    sock->SendStringList(strlist);
    return true;
}

BaseSocketHandler::BaseSocketHandler(MainServer *parent, MythSocket *sock,
        QString hostname) : ProtoSocketHandler(parent, sock, hostname)
{
}

BaseSocketHandler::BaseSocketHandler(BaseSocketHandler &other) : 
        ProtoSocketHandler(other)
{
}

void BaseSocketHandler::Shutdown(void)
{
    SendMythSystemEvent(QString("CLIENT_DISCONNECTED HOSTNAME %1")
                                    .arg(m_hostname));
}
