#ifndef JOBSOCKETHANDLER_H_
#define JOBSOCKETHANDLER_H_

#include <QMap>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "mythsocketmanager.h"
#include "mythsocket.h"
#include "jobinforun.h"

typedef QMap<int, JobInfoRun*> JobMap;

class JobSocketHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    JobSocketHandler();
   ~JobSocketHandler();

    bool HandleQuery(MythSocket *socket, QStringList &commands,
                                QStringList &slist);
    QString GetHandlerName(void)            { return "JOBQUEUE"; }
    void SetParent(MythSocketManager *parent);
    void connectionClosed(MythSocket *sock);

  protected slots:
    void MasterReconnect(void);

  private:
    bool HandleRunJob(MythSocket *socket, JobInfoRun &job);
    bool HandlePauseJob(MythSocket *socket, JobInfoRun *job);
    bool HandleResumeJob(MythSocket *socket, JobInfoRun *job);
    bool HandleStopJob(MythSocket *socket, JobInfoRun *job);
    bool HandleRestartJob(MythSocket *socket, JobInfoRun *job);
    bool HandlePollJob(MythSocket *socket, JobInfoRun *job);

    JobMap          m_jobMap;
    QReadWriteLock  m_jobLock;
    MythSocket     *m_serverSock; 
    QTimer         *m_masterReconnect;
};

#endif
