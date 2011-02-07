using namespace std;

// Qt
#include <QReadWriteLock>
#include <QMap>
#include <QList>
#include <QWaitCondition>

// MythTV
#include "mythsocket.h"
#include "handler.h"
#include "mythdeque.h"

class ProcessRequestThread;
class MythServer;

typedef QMap<QString, ProtoSocketHandler*>  SockHandlerMap;
typedef QMap<QString, ProtoHandler*>        HandlerMap;
typedef QList<ProtoSocketHandler*>          SockHandlerList;

typedef struct socketentry {
    MythSocket     *socket;
    QString         hostname;
    bool            expectingresponse;
    SockHandlerMap  handlers;
} SocketEntry;

typedef QMap<MythSocket*, socketentry*>      SockMap;

class MainServer : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MainServer(int port);
   ~MainServer();

    bool      registerHandler(ProtoHandler *handler);
    bool    unregisterHandler(QString &name);
    void unregisterAllHandler();

    void readyRead(MythSocket *socket);
    void connectionClosed(MythSocket *socket);
    void connectionFailed(MythSocket *socket) { (void)socket; }
    void connected(MythSocket *socket) { (void)socket; }

    void ProcessRequest(MythSocket *sock);
    void MarkUnused(ProcessRequestThread *prt);

    void   ExpectingResponse(MythSocket *sock, bool expecting);
    bool isExpectingResponse(MythSocket *sock);

    ProtoSocketHandler *GetHandlerSock(QString hostname, QString type);
    ProtoSocketHandler *GetHandlerSock(QString hostname);
    ProtoSocketHandler *GetHandlerSock(MythSocket *sock, QString type);
    ProtoSocketHandler *GetHandlerSock(MythSocket *sock);
    SockHandlerList    *GetSockHandlerList(MythSocket *sock);

    bool    AddHandlerSock(ProtoSocketHandler *sock);
    bool DeleteHandlerSock(ProtoSocketHandler *sock);
    bool DeleteHandlerSock(MythSocket *sock, QString type);
    int  DeleteHandlerSock(MythSocket *sock, bool force=true);

    int GetExitCode() const { return m_exitCode; }

  private slots:
    void newConnection(MythSocket *);

  private:
    MythServer     *m_mythserver;

    QReadWriteLock  m_handlerLock;
    HandlerMap      m_handlerMap;

    QReadWriteLock  m_sockHandlerLock;
    SockMap         m_sockHandlerMap;

    QMutex                            m_threadPoolLock;
    QWaitCondition                    m_threadPoolCond;
    MythDeque<ProcessRequestThread *> m_threadPool;

    int m_exitCode;

    void ProcessRequestWork(MythSocket *sock);
    void HandleVersion(MythSocket *sock, const QStringList &slist);
    void HandleAnnounce(MythSocket *sock, QStringList &commands,
                                          QStringList &slist);
    void HandleDone(MythSocket *sock);
    void SetExitCode(int exitCode, bool closeApplication);
};
