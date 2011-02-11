using namespace std;

// Qt 
#include <QMap>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QThread>
#include <QEvent>
#include <QTimer>
#include <QCoreApplication>

// MythTV
#include "server.h"
#include "handler.h"
#include "basehandler.h"
#include "mainserver.h"
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "mythconfig.h"
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"

/** Milliseconds to wait for an existing thread from
 *  process request thread pool.
 */
#define PRT_TIMEOUT 10
/** Number of threads in process request thread pool at startup. */
#define PRT_STARTUP_THREAD_COUNT 5

#define LOC      QString("MainServer: ")
#define LOC_WARN QString("MainServer, Warning: ")
#define LOC_ERR  QString("MainServer, Error: ")

const uint MainServer::kMasterServerReconnectTimeout = 1000;

class ProcessRequestThread : public QThread
{
  public:
    ProcessRequestThread(MainServer *ms):
        m_parent(ms), m_socket(NULL), m_threadlives(false) {}

    void setup(MythSocket *sock)
    {
        QMutexLocker locker(&m_lock);
        m_socket = sock;
        m_socket->UpRef();
        m_waitCond.wakeAll();
    }

    void killit(void)
    {
        QMutexLocker locker(&m_lock);
        m_threadlives = false;
        m_waitCond.wakeAll();
    }

    virtual void run(void)
    {
        QMutexLocker locker(&m_lock);
        m_threadlives = true;
        m_waitCond.wakeAll(); // Signal to creating thread

        while (true)
        {
            m_waitCond.wait(locker.mutex());

            if (!m_threadlives)
                break;

            if (!m_socket)
                continue;

            m_parent->ProcessRequest(m_socket);
            m_socket->DownRef();
            m_socket = NULL;
            m_parent->MarkUnused(this);
        }
    }

    QMutex m_lock;
    QWaitCondition m_waitCond;

  private:
    MainServer *m_parent;
    MythSocket *m_socket;
    bool        m_threadlives;
};

/** \class MainServer
 *  \brief Primary thread for running a socket server and listening
 *         for MythProtocol queries.
 */

/** \fn MythServer::MythServer(int port)
 *  \brief Creates a MainServer class, opening the socket and spawning
 *         threads to listen for traffic.
 *
 *  \param port     Integer port for server to listen on
 */
MainServer::MainServer(bool isMaster, int port) :
        m_mythserver(NULL),
        m_sockHandlerLock(QReadWriteLock::Recursive),
        m_exitCode(BACKEND_EXIT_OK),
        m_masterServerReconnect(NULL),
        m_isMaster(isMaster)
{
    for (int i = 0; i < PRT_STARTUP_THREAD_COUNT; i++)
    {
        // spawn requests threads and store
        ProcessRequestThread *prt = new ProcessRequestThread(this);
        prt->m_lock.lock();
        prt->start();
        prt->m_waitCond.wait(&prt->m_lock);
        prt->m_lock.unlock();
        m_threadPool.push_back(prt);
    }

    m_mythserver = new MythServer();
    if (!m_mythserver->listen(QHostAddress::Any, port))
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to bind port %1. Exiting.")
                .arg(port));
        SetExitCode(BACKEND_BUGGY_EXIT_NO_BIND_MAIN, false);
        return;
    }

    connect(m_mythserver, SIGNAL(newConnect(MythSocket *)),
                        SLOT(newConnection(MythSocket *)));

    gCoreContext->addListener(this);
    gCoreContext->SetBackend(true);

    BaseProtoHandler *handler = new BaseProtoHandler();
    registerHandler(dynamic_cast<ProtoHandler*>(handler));

    if (!isMaster)
        masterConnect();
}

/** \fn MainServer::~MainServer()
 *  \brief MainServer destructor tells socketserver to disconnect and
 *         terminate.  Handler threads will automatically close on their own.
 */
MainServer::~MainServer()
{
    if (m_mythserver)
    {
        m_mythserver->disconnect();
        m_mythserver->deleteLater();
        m_mythserver = NULL;
    }

    unregisterAllHandler();
}

void MainServer::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;

        QString message = me->Message();

        if (message == "CLEAR_SETTINGS_CACHE")
        {
            gCoreContext->ClearSettingsCache();
            BroadcastEvent(me);
        }
        else if (message.startsWith("SYSTEM_EVENT "))
            BroadcastSystemEvent(me);
        else if (message == "LOCAL_RECONNECT_TO_MASTER")
            masterReconnect();
    }
}

void MainServer::BroadcastEvent(MythEvent *me)
{
    SockHandlerList socklist;
    ProtoSocketHandler *sock;
    QStringList message;

    // only broadcast messages which originate locally
    if (!me->isLocalOrigin())
        return;

    if (!isMaster())
    {
        // because i cant figure out how to prevent it from endlessly looping
        message << "MESSAGE";
        SockHandlerList *slist = GetSockHandlerList(QString("UPSTREAM"));
        if (slist == NULL)
        {
            VERBOSE(VB_IMPORTANT, "MainServer::BroadcastEvent : could "
                                  "not find master backend");
            return;
        }

        sock = slist->first();
        delete slist;
        
        sock->UpRef();
        socklist << sock;
    }
    else
    {
        QReadLocker rlock(&m_sockHandlerLock);
        SockMap::const_iterator i = m_sockHandlerMap.begin();
        while (i != m_sockHandlerMap.end())
        {
            socketentry *entry = *i;
            SockHandlerMap::const_iterator j = entry->handlers.begin();
            while (j != entry->handlers.end())
            {
                sock = j.value();
                if (sock->wantsNonSystemEvents())
                {
                    sock->UpRef();
                    socklist << sock;
                    sock = NULL;
                    break;
                }
                sock = NULL;
                j++;
            }
            i++;
        }
    }

    message << me->Message()
            << me->ExtraDataList();

    while (!socklist.isEmpty())
    {
        sock = socklist.takeFirst();
        if(isMaster())
            sock->SendStringList(message);
        else
        {
            sock->SendReceiveStringList(message);
            if (message.first() != "OK")
                VERBOSE(VB_IMPORTANT, "MainServer::BroadcastEvent failed");
        }
        sock->DownRef();
    }
}

void MainServer::BroadcastSystemEvent(MythEvent *me)
{
    SockHandlerList socklist;
    ProtoSocketHandler *sock;
    QStringList hostlist(gCoreContext->GetHostName());
    QStringList message;

    // only broadcast messages which originate locally
    if (!me->isLocalOrigin())
        return;

    if (!isMaster())
    {
        // because i cant figure out how to prevent it from endlessly looping
        message << "MESSAGE";
        SockHandlerList *slist = GetSockHandlerList(QString("UPSTREAM"));
        if (slist == NULL)
        {
            VERBOSE(VB_IMPORTANT, "MainServer::BroadcastSystemEvent : could "
                                  "not find master backend");
            return;
        }

        sock = slist->first();
        delete slist;
        
        sock->UpRef();
        socklist << sock;
    }
    else
    {
        message << "BACKEND_MESSAGE";
        QReadLocker rlock(&m_sockHandlerLock);
        SockMap::const_iterator i = m_sockHandlerMap.begin();
        while (i != m_sockHandlerMap.end())
        {
            socketentry *entry = *i;
            if (hostlist.contains(entry->hostname))
                continue;

            SockHandlerMap::const_iterator j = entry->handlers.begin();
            while (j != entry->handlers.end())
            {
                sock = j.value();

                if (!sock->wantsSystemEvents())
                {
                    // system events not wanted
                    j++;
                    continue;
                }

                if (sock->wantsNonSystemEvents())
                {
                    // normal behavior, send once per host
                    // if only system events are wanted, send regardless
                    if (hostlist.contains(entry->hostname))
                    {
                        j++;
                        continue;
                    }
                    hostlist << entry->hostname;
                }

                sock->UpRef();
                socklist << sock;
                sock = NULL;
                j++;
            }
            i++;
        }
    }

    message << me->Message()
            << me->ExtraDataList();

    while (!socklist.isEmpty())
    {
        sock = socklist.takeFirst();
        if (isMaster())
            sock->SendStringList(message);
        else
        {
            sock->SendReceiveStringList(message);
            if (message.first() != "OK")
                VERBOSE(VB_IMPORTANT, "MainServer::BroadcastSystemEvent failed");
        }
        sock->DownRef();
    }
}

// add a new named command handler
bool MainServer::registerHandler(ProtoHandler *handler)
{
    QString name = handler->getType();
    {
        QReadLocker rlock(&m_handlerLock);
        if (m_handlerMap.contains(name))
        {
            VERBOSE(VB_IMPORTANT, "MainServer::registerHandler: " + name +
                                  " already registered!");
            m_handlerLock.unlock();
            return false;
        }
    }

    QWriteLocker wlock(&m_handlerLock);
    m_handlerMap.insert(name, handler);
    VERBOSE(VB_IMPORTANT, "MainServer::registerHandler: registering " + name);
    return true;
}

// remove a named command handler
bool MainServer::unregisterHandler(QString &name)
{
    {
        QReadLocker rlock(&m_handlerLock);
        if (!m_handlerMap.contains(name))
        {
            VERBOSE(VB_IMPORTANT, "MainServer::unregisterHandler: " + name +
                                  " is not currently registered!");
            m_handlerLock.unlock();
            return false;
        }
    }

    QWriteLocker wlock(&m_handlerLock);
    ProtoHandler *handler = m_handlerMap.take(name);
    handler->deleteLater();

    VERBOSE(VB_IMPORTANT, "MainServer::unregisterHandler: unregistering "
                          + name);
    return true;
}

// remove all handlers in preparation for shutdown
void MainServer::unregisterAllHandler()
{
    QWriteLocker wlock(&m_handlerLock);
    
    HandlerMap::iterator i;
    for (i = m_handlerMap.begin(); i != m_handlerMap.end(); ++i)
        (i.value())->deleteLater();

    m_handlerMap.clear();
}

void MainServer::newConnection(MythSocket *socket)
{
    socket->setCallbacks(this);
}

void MainServer::connectionClosed(MythSocket *socket)
{
    SockHandlerList *slist = GetSockHandlerList(socket);
    if (slist == NULL)
        return;

    ProtoSocketHandler *handler = slist->first();
    if (handler->getType() == "UPSTREAM")
    {
        handler->DownRef();
        m_masterServerReconnect->start(kMasterServerReconnectTimeout);
    }
    else
    {
        SockHandlerList::const_iterator i = slist->begin();
        while (i != slist->end())
        {
            handler = *i;
            handler->Shutdown();
            handler->DownRef();
            i++;
        }
    }

    delete slist;
}

void MainServer::masterConnect(void)
{
    SockHandlerList *slist = GetSockHandlerList(QString("UPSTREAM"));
    if (slist != NULL)
    {
        delete slist;
        return;
    }

    if (m_masterServerReconnect == NULL)
    {
        m_masterServerReconnect = new QTimer(this);
        m_masterServerReconnect->setSingleShot(true);
        connect(m_masterServerReconnect, SIGNAL(timeout()), this,
                SLOT(masterReconnect()));
    }

    MythSocket *masterServer = new MythSocket();
    QString address = gCoreContext->GetSetting("MasterServerIP", "127.0.0.1");
    int port = gCoreContext->GetNumSetting("MasterServerPort", 6543);

    VERBOSE(VB_IMPORTANT, QString("Connecting to master server: %1:%2")
                           .arg(address).arg(port));

    if (!masterServer->connect(address, port))
    {
        VERBOSE(VB_IMPORTANT, "Connection to master server timed out.");
        masterServer->DownRef();
        m_masterServerReconnect->start(kMasterServerReconnectTimeout);
        return;
    }

    if (masterServer->state() != MythSocket::Connected)
    {
        VERBOSE(VB_IMPORTANT, "Could not connect to master server.");
        masterServer->DownRef();
        m_masterServerReconnect->start(kMasterServerReconnectTimeout);
        return;
    }

    if (!gCoreContext->CheckProtoVersion(masterServer))
    {
        masterServer->DownRef();
        m_masterServerReconnect->start(kMasterServerReconnectTimeout);
        return;
    }

    VERBOSE(VB_IMPORTANT, "Connected successfully");

    QString hostname = gCoreContext->GetHostName();
    UpstreamSocketHandler *ps = new UpstreamSocketHandler(this,
                                        masterServer, hostname);

    QStringList strlist( QString("ANN Monitor %1 %2")
                            .arg(gCoreContext->GetHostName())
                            .arg(true));
    if (!ps->SendReceiveStringList(strlist) ||
        strlist.empty() || strlist[0] == "ERROR")
    {
        ps->DownRef();
        ps = NULL;
        if (strlist.empty())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to open master server socket, timeout");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to open master server socket" +
                    ((strlist.size() >= 2) ?
                     QString(", error was %1").arg(strlist[1]) :
                     QString(", remote error")));
        }
        m_masterServerReconnect->start(kMasterServerReconnectTimeout);
        return;
    }

    AddHandlerSock(ps);
    masterServer->setCallbacks(this);
    masterServer->DownRef();
    masterServer = NULL;
    ps->DownRef();
    ps = NULL;

    {
        QReadLocker rlock(&m_handlerLock);
        HandlerMap::const_iterator i = m_handlerMap.begin();
        while (i != m_handlerMap.end())
        {
            (i.value())->MasterConnected(this);
            i++;
        }
    }
}

void MainServer::masterDisconnect(void)
{
    SockHandlerList *slist = GetSockHandlerList(QString("UPSTREAM"));
    if (slist == NULL)
        return;
    ProtoSocketHandler *ps = slist->first();
    delete slist;
    slist = NULL;

    delete m_masterServerReconnect;
    m_masterServerReconnect = NULL;

    DeleteHandlerSock(ps);
    {
        QReadLocker rlock(&m_handlerLock);
        HandlerMap::const_iterator i = m_handlerMap.begin();
        while (i != m_handlerMap.end())
        {
            (i.value())->MasterDisconnected(this);
            i++;
        }
    }
}

void MainServer::masterReconnect(void)
{
    masterConnect();
}

void MainServer::readyRead(MythSocket *sock)
{
    if (isExpectingResponse(sock))
        return;

    ProcessRequestThread *prt = NULL;
    {
        QMutexLocker locker(&m_threadPoolLock);

        if (m_threadPool.empty())
        {
            VERBOSE(VB_GENERAL, "Waiting for a process request thread.. ");
            m_threadPoolCond.wait(&m_threadPoolLock, PRT_TIMEOUT);
        }

        if (!m_threadPool.empty())
        {
            prt = m_threadPool.front();
            m_threadPool.pop_front();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Adding a new process request thread");
            prt = new ProcessRequestThread(this);
            prt->m_lock.lock();
            prt->start();
            prt->m_waitCond.wait(&prt->m_lock);
            prt->m_lock.unlock();
        }
    }

    prt->setup(sock);
}

void MainServer::MarkUnused(ProcessRequestThread *prt)
{
    QMutexLocker locker(&m_threadPoolLock);
    m_threadPool.push_back(prt);
    m_threadPoolCond.wakeAll();
}

void MainServer::ExpectingResponse(MythSocket *sock, bool expecting)
{
    QWriteLocker wlock(&m_sockHandlerLock);
    if (!(m_sockHandlerMap.contains(sock)))
        return;

    m_sockHandlerMap[sock]->expectingresponse = expecting;
}

bool MainServer::isExpectingResponse(MythSocket *sock)
{
    QReadLocker rlock(&m_sockHandlerLock);
    if (!(m_sockHandlerMap.contains(sock)))
        return false;

    return m_sockHandlerMap[sock]->expectingresponse;
}

// add a new handler socket, or update the struct holding an existing one
bool MainServer::AddHandlerSock(ProtoSocketHandler *sock)
{
    QWriteLocker wlock(&m_sockHandlerLock);

    socketentry *entry = NULL;
    MythSocket *ms = sock->getSocket();

    if (m_sockHandlerMap.contains(ms))
        entry = m_sockHandlerMap[ms];

    if (!entry)
    {
        entry = new socketentry;
        entry->socket               = ms;
        entry->hostname             = sock->getHostname();
        entry->expectingresponse    = false;
        m_sockHandlerMap.insert(ms, entry);
    }

    QString type = sock->getType();
    if (entry->handlers.contains(type))
    {
        VERBOSE(VB_IMPORTANT, "MainServer: Trying to add "
                              "duplicate socket handler");
        return false;
    }

    VERBOSE(VB_SOCKET, QString("MainServer: Adding socket handler (%1)")
                            .arg(sock->getType()));
    entry->handlers.insert(type, sock);
    sock->UpRef();
    return true;
}

// delete a handler socket directly
bool MainServer::DeleteHandlerSock(ProtoSocketHandler *sock)
{
    return DeleteHandlerSock(sock->getSocket(),
                             sock->getType());
}

// delete a handler socket mapped to given MythSocket and type
bool MainServer::DeleteHandlerSock(MythSocket *sock, QString type)
{
    QWriteLocker wlock(&m_sockHandlerLock);

    // check for matching socket
    if (!(m_sockHandlerMap.contains(sock)))
        return false;
    socketentry *entry = m_sockHandlerMap[sock];

    // check for matching socket handler
    if (!(entry->handlers.contains(type)))
        return false;
    (entry->handlers.take(type))->deleteLater();


    // flush socket if empty
    DeleteHandlerSock(sock, false);
    return true;
}

// delete all handler sockets for given MythSocket
int MainServer::DeleteHandlerSock(MythSocket *sock, bool force)
{
    QWriteLocker wlock(&m_sockHandlerLock);
    int count = 0;

    // check for matching sockets
    if (!(m_sockHandlerMap.contains(sock)))
        return count;
    socketentry *entry = m_sockHandlerMap[sock];
    
    // check for empty handlers list
    if (!(entry->handlers.isEmpty()))
    {
        // not allowed to clear remaining handlers
        if (!force)
            return count;

        // clear remaining handlers
        count = entry->handlers.size();
        SockHandlerMap::iterator i;
        for (i = entry->handlers.begin(); i != entry->handlers.end(); ++i)
            (i.value())->deleteLater();
    }

    m_sockHandlerMap.remove(sock);
    delete entry;
    return count;
}

// get a handler socket from given hostname and type
// used for specific communications methods defined in the handler
ProtoSocketHandler *MainServer::GetHandlerSock(QString hostname, QString type)
{
    QReadLocker rlock(&m_sockHandlerLock);
    ProtoSocketHandler *handler = NULL;
    socketentry *entry = NULL;

    SockMap::const_iterator i;
    for (i = m_sockHandlerMap.begin(); i != m_sockHandlerMap.end(); ++i)
    {
        entry = i.value();
        if (entry->hostname == hostname)
        {
            if (entry->handlers.contains(type))
            {
                handler = entry->handlers[type];
                break;
            }
        }
    }
    return handler;
}

// get a handler socket from given hostname of any type
// used for sending system events
ProtoSocketHandler *MainServer::GetHandlerSock(QString hostname)
{
    QReadLocker rlock(&m_sockHandlerLock);
    ProtoSocketHandler *handler = NULL;
    socketentry *entry = NULL;

    SockMap::const_iterator i;
    for (i = m_sockHandlerMap.begin(); i != m_sockHandlerMap.end(); ++i)
    {
        entry = i.value();
        if (entry->hostname == hostname)
        {
            SockHandlerMap::const_iterator j = entry->handlers.begin();
            return (++j).value();
        }

    }

    return handler;
}

// get a handler socket from given MythSocket and type
ProtoSocketHandler *MainServer::GetHandlerSock(MythSocket *sock, QString type)
{
    QReadLocker rlock(&m_sockHandlerLock);
    ProtoSocketHandler *handler = NULL;

    if (!(m_sockHandlerMap.contains(sock)))
        return handler;
    socketentry *entry = m_sockHandlerMap[sock];

    if (!(entry->handlers.contains(type)))
        return handler;
    return entry->handlers[type];
}

// get a handler socket from a given MythSocket
ProtoSocketHandler *MainServer::GetHandlerSock(MythSocket *sock)
{
    QReadLocker rlock(&m_sockHandlerLock);
    ProtoSocketHandler *handler = NULL;

    if (!(m_sockHandlerMap.contains(sock)))
    {
        return handler;
    }
    socketentry *entry = m_sockHandlerMap[sock];

    if (entry->handlers.isEmpty())
    {
        return handler;
    }

    SockHandlerMap::const_iterator i = entry->handlers.begin();
    return i.value();
}

SockHandlerList *MainServer::GetSockHandlerList(MythSocket *sock)
{
    QReadLocker rlock(&m_sockHandlerLock);
    SockHandlerList *slist = NULL;

    if (!m_sockHandlerMap.contains(sock))
        return slist;

    slist = new SockHandlerList();

    socketentry *entry = m_sockHandlerMap[sock];
    SockHandlerMap::const_iterator i = entry->handlers.begin();
    while (i != entry->handlers.end())
    {
        slist->append(i.value());
        i++;
    }

    return slist;
}

SockHandlerList *MainServer::GetSockHandlerList(QString type)
{
    QReadLocker rlock(&m_sockHandlerLock);
    SockHandlerList *slist = new SockHandlerList();

    SockMap::const_iterator i = m_sockHandlerMap.begin();
    SockHandlerMap::const_iterator j;

    while (i != m_sockHandlerMap.end())
    {
        socketentry *entry = i.value();
        j = entry->handlers.begin();
        while (j != entry->handlers.end())
        {
            if (j.key() == type)
                slist->append(j.value());
            j++;
        }
        i++;
    }

    if (slist->isEmpty())
    {
        delete slist;
        slist = NULL;
    }

    return slist;
}

/** \fn MainServer::ProcessRequest()
 *  \brief Called by ProcessRequestThread when it acquires a socket
 *         for processing.
 *
 *  \param sock     MythSocket containing the active connection
 *                  to be processed.

 */
void MainServer::ProcessRequest(MythSocket *sock)
{
    sock->Lock();

    if (sock->bytesAvailable() > 0)
        ProcessRequestWork(sock);

    sock->Unlock();
}

/** \fn MainServer::ProcessRequestWork(MythSocket *sock)
 *  \brief Called by ProcessRequest if data is availble on the socket.
 *
 *  \param sock     MythSocket containing the active connection
 *                  to be processed.
 */
void MainServer::ProcessRequestWork(MythSocket *sock)
{
    QStringList listline;
    if (!sock->readStringList(listline))
        return;

    QString line = listline[0];

    line = line.simplified();
    QStringList tokens = line.split(' ', QString::SkipEmptyParts);
    QString command = tokens[0];

    bool handled = false;

    if (command == "DONE")
    {
        HandleDone(sock);
        handled = true;
    }
    else if (command == "OK" || command == "UNKNOWN_COMMAND")
    {
        VERBOSE(VB_IMPORTANT, QString("Got '%1' out of sequence.")
                                    .arg(command));
        handled = true;
    }
    else if (command == "MYTH_PROTO_VERSION")
    {
        HandleVersion(sock, tokens);
        handled = true;
    }
    else if (!(sock->isValidated()))
    {
        VERBOSE(VB_IMPORTANT, QString("Got '%1' on unvalidated connection.")
                                    .arg(command));
        QStringList sl; sl << "ERROR"
                           << "Command sent before proto version validation";
        sock->writeStringList( sl );
        handled = true;
    }

    // commands below here require a validated connection
    else if (command == "ANN")
    {
        // loop through registered handlers until one handles the announce
        QReadLocker rlock(&m_handlerLock);
        HandlerMap::iterator i = m_handlerMap.begin();
        while (i != m_handlerMap.end())
        {
            handled = (i.value())->HandleAnnounce(this, sock, tokens,
                                                  listline);
            i++;

            if (handled)
                break;
        }
    }
    else
    {
        // commands below here require an announced connection
        ProtoSocketHandler *ps = GetHandlerSock(sock);
        if (ps == NULL)
        {
            VERBOSE(VB_IMPORTANT, QString("Got '%1' on unannounced connection.")
                                    .arg(command));
            QStringList sl; sl << "ERROR"
                               << "Command sent before announce";
            sock->writeStringList( sl );
            handled = true;
        }
        else
        {
            ps->UpRef();

            if (command == "MESSAGE")
            {
                HandleMessage(ps, listline);
                handled = true;
            }
            else if (command == "BACKEND_MESSAGE")
            {
                HandleBackendMessage(ps, listline);
                handled = true;
            }
            else
            {
                // loop through registered handlers until one handles the command
                QReadLocker rlock(&m_handlerLock);
                HandlerMap::iterator i = m_handlerMap.begin();
                while (i != m_handlerMap.end())
                {
                    handled = (i.value())->HandleCommand(this, sock, tokens,
                                                          listline);
                    i++;

                    if (handled)
                        break;
                }
            }
            ps->DownRef();
        }
    }

    if (!handled)
    {
        VERBOSE(VB_IMPORTANT, "Unknown command: " + command);
        QStringList sl; sl << "UNKNOWN_COMMAND";
        sock->writeStringList( sl );
    }
}

/**
 * \addtogroup myth_network_protocol
 * \par        MYTH_PROTO_VERSION \e version \e token
 * Checks that \e version and \e token match the backend's version.
 * If it matches, the stringlist of "ACCEPT" \e "version" is returned.
 * If it does not, "REJECT" \e "version" is returned,
 * and the socket is closed (for this client)
 */
void MainServer::HandleVersion(MythSocket *socket, const QStringList &slist)
{
    QString cLOC("MainServer::HandleVersion:");
    QStringList reject; reject << "REJECT" << MYTH_PROTO_VERSION;
    QStringList accept; accept << "ACCEPT" << MYTH_PROTO_VERSION;

    if (socket->isValidated())
    {
        socket->writeStringList(accept);
        return;
    }

    QString version = slist[1];
    if (version != MYTH_PROTO_VERSION)
    {
        VERBOSE(VB_GENERAL, cLOC + "Client speaks protocol version " + version +
                " but we speak " + MYTH_PROTO_VERSION + "!");
        socket->writeStringList(reject);
        HandleDone(socket);
        return;
    }

    if (slist.size() < 3)
    {
        VERBOSE(VB_GENERAL, cLOC + " Client did not pass proper argument " +
                "count. Refusing connection!");
        socket->writeStringList(reject);
        HandleDone(socket);
        return;
    }

    QString token = slist[2];
    if (token != MYTH_PROTO_TOKEN)
    {
        VERBOSE(VB_GENERAL, cLOC + "Client sent protocol token " + token +
                " but we use " + MYTH_PROTO_TOKEN + "!");
        socket->writeStringList(reject);
        HandleDone(socket);
        return;
    }

    socket->writeStringList(accept);
    socket->setValidated();
}

/**
 * \addtogroup myth_network_protocol
 * \par        DONE
 * Closes this client's socket.
 */
void MainServer::HandleDone(MythSocket *socket)
{
    socket->close();
//    DeleteHandlerSock(socket);
}

void MainServer::HandleMessage(ProtoSocketHandler *sock,
                               QStringList &listline)
{
    QStringList sl;
    if (listline.size() < 2)
    {
        sl << "ERROR" << "no message in command";
        sock->SendStringList(sl);
        return;
    }

    QString message = listline[1];
    QStringList extra;
    for (int i = 2; i < listline.size(); i++)
        extra << listline[i];

    if (extra.empty())
    {
        MythEvent me(message);
        gCoreContext->dispatch(me);
    }
    else
    {
        MythEvent me(message, extra);
        gCoreContext->dispatch(me);
    }

    sl << "OK";
    sock->SendStringList(sl);
}

void MainServer::HandleBackendMessage(ProtoSocketHandler *sock,
                                      QStringList &listline)
{
    QString message = listline[1];
    QStringList extra;
    for (int i = 2; i < listline.size(); i++)
        extra << listline[i];
    MythEvent me(message, extra);
    me.LocalOrigin(false);
    gCoreContext->dispatch(me);
}

void MainServer::SetExitCode(int exitCode, bool closeApplication)
{
    m_exitCode = exitCode;
    if (closeApplication)
        QCoreApplication::exit(m_exitCode);
}

