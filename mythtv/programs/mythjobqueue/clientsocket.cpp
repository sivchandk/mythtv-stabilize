
#include <cstdlib>
#include <memory>

using namespace std;

#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "clientsocket.h"
#include "mythverbose.h"
#include "mythcorecontext.h"
#include "util.h"

bool ClientSocketHandler::HandleAnnounce(MythSocket *socket,
                    QStringList &commands, QStringList &slist)
{
    if (commands[1] == "Playback" || commands[1] == "Monitor")
    {
        if (commands.size() < 4)
        {
            VERBOSE(VB_IMPORTANT, QString("Received malformed ANN %1 query")
                    .arg(commands[1]));
            slist.clear();
            slist << "ERROR" << "malformed_ann_query";
            socket->writeStringList(slist);
            return true;
        }

        socket->setAnnounce(slist);
        VERBOSE(VB_IMPORTANT, QString("adding: %1 as a client")
                             .arg(commands[2]));
        // send mythsystem event
        slist.clear();
        slist << "OK";
        socket->writeStringList(slist);
        return true;
    }

    return false;
}

bool ClientSocketHandler::HandleQuery(MythSocket *socket,
                    QStringList &commands, QStringList &slist)
{
    bool handled = false;

    if (commands[0] == "SET_VERBOSE")
        handled = HandleSetVerbose(socket, slist);
    else if (commands[0] == "QUERY_LOAD")
        handled = HandleQueryLoad(socket);
    else if (commands[0] == "QUERY_UPTIME")
        handled = HandleQueryUptime(socket);
    else if (commands[0] == "QUERY_HOSTNAME")
        handled = HandleQueryHostname(socket);
    else if (commands[0] == "QUERY_MEMSTATS")
        handled = HandleQueryMemStats(socket);
    else if (commands[0] == "QUERY_TIME_ZONE")
        handled = HandleQueryTimeZone(socket);

    return handled;
}


bool ClientSocketHandler::HandleSetVerbose(MythSocket *socket,
                                           QStringList &slist)
{
    QStringList res;
    
    if (slist.size() != 2)
    {
        res << "ERROR" << "invalid SET_VERBOSE call";
        socket->writeStringList(res);
        return true;
    }

    QString newverbose = slist[1];
    if (newverbose.length() > 12)
    {
        parse_verbose_arg(newverbose.right(newverbose.length()-12));
        VERBOSE(VB_IMPORTANT, QString("Verbose level changed, new level is: %1")
                                .arg(verboseString));
        res << "OK";
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Invalid SET_VERBOSE string: '%1'")
                                .arg(newverbose));
        res << "ERROR" << "invalid SET_VERBOSE string";
    }

    socket->writeStringList(res);
    return true;
}

bool ClientSocketHandler::HandleQueryLoad(MythSocket *socket)
{
    QStringList strlist;

    double loads[3];
    if (getloadavg(loads,3) == -1)
    {
        strlist << "ERROR" << "getloadavg() failed";
    }
    else
        strlist << QString::number(loads[0])
                << QString::number(loads[1])
                << QString::number(loads[2]);

    socket->writeStringList(strlist);
    return true;
}

bool ClientSocketHandler::HandleQueryUptime(MythSocket *socket)
{
    QStringList strlist;
    time_t      uptime;

    if (getUptime(uptime))
        strlist << QString::number(uptime);
    else
    {
        strlist << "ERROR" << "Could not determine uptime.";
    }

    socket->writeStringList(strlist);
    return true;
}

bool ClientSocketHandler::HandleQueryHostname(MythSocket *socket)
{
    QStringList strlist;

    strlist << gCoreContext->GetHostName();

    socket->writeStringList(strlist);
    return true;
}

bool ClientSocketHandler::HandleQueryMemStats(MythSocket *socket)
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

    socket->writeStringList(strlist);
    return true;
}

bool ClientSocketHandler::HandleQueryTimeZone(MythSocket *socket)
{
    QStringList strlist;
    strlist << getTimeZoneID() << QString::number(calc_utc_offset())
            << mythCurrentDateTime().toString(Qt::ISODate);

    socket->writeStringList(strlist);
    return true;
}

