#ifndef _MYTHSOCKETMANAGER_H_
#define _MYTHSOCKETMANAGER_H_

using namespace std;

// Qt
#include <QReadWriteLock>
#include <QMap>
#include <QList>
#include <QTimer>
#include <QWaitCondition>
#include <QTcpServer>

// MythTV
#include "mythsocket.h"
#include "mythdeque.h"

class ProcessRequestThread;
class MythSocketManager;

typedef QMap<QString, MythSocket*> SockMap;

class MythServer : public QTcpServer
{
    Q_OBJECT

  signals:
    void newConnect(MythSocket*);

  protected:
    virtual void incomingConnection(int socket);
};

class SocketRequestHandler : public QObject
{
    Q_OBJECT
  public:
    SocketRequestHandler() {};
   ~SocketRequestHandler() {};

    virtual bool HandleAnnounce(MythSocket *socket, QStringList &commands,
                                QStringList &slist)
                    { return false; }
    virtual bool HandleQuery(MythSocket *socket, QStringList &commands,
                                QStringList &slist)
                    { return false; }
    virtual QString GetHandlerName(void)                { return "BASE"; }
    virtual void connectionAnnounced(MythSocket *socket, QStringList &commands,
                                QStringList &slist)     { (void)socket; }
    virtual void connectionClosed(MythSocket *socket)   { (void)socket; }
    virtual void SetParent(MythSocketManager *parent)   { m_parent = parent; }

  protected:
    MythSocketManager *m_parent;
};

typedef QMap<QString, SocketRequestHandler*> SockHandlerMap;

class MBASE_PUBLIC MythSocketManager : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MythSocketManager();
   ~MythSocketManager();

    void readyRead(MythSocket *socket);
    void connectionClosed(MythSocket *socket);
    void connectionFailed(MythSocket *socket) { (void)socket; }
    void connected(MythSocket *socket) { (void)socket; }

    void SetThreadCount(uint count);
    void MarkUnused(ProcessRequestThread *prt);

    bool        AddConnection(QString name, MythSocket *socket);
    QString     AddConnection(MythSocket *socket);
    MythSocket *GetConnection(QString name);

    void ProcessRequest(MythSocket *socket);

    void RegisterHandler(SocketRequestHandler *handler);
    bool Listen(int port);

  private:
    void ProcessRequestWork(MythSocket *socket);
    void HandleVersion(MythSocket *socket, const QStringList slist);
    void HandleDone(MythSocket *socket);

    MythDeque<ProcessRequestThread *>   m_threadPool;
    QMutex                              m_threadPoolLock;
    QWaitCondition                      m_threadPoolCond;

    SockMap         m_socketMap;
    QReadWriteLock  m_socketLock;

    SockHandlerMap  m_handlerMap;
    QReadWriteLock  m_handlerLock;

    MythServer     *m_server;
};
#endif
