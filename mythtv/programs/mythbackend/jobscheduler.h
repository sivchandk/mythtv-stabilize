#ifndef JOBSCHEDULER_H_
#define JOBSCHEDULER_H_

using namespace std;

#include <QMap>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QReadWriteLock>

#include "mythsocketmanager.h"
#include "mythsocket.h"
#include "programinfo.h"
#include "jobinfodb.h"

class JobQueueSocket;

typedef QMap<QString, JobQueueSocket*> JobHostMap;
typedef QMap<int, JobCommandDB*> JobCommandMap;

typedef QList<JobQueueSocket*> JobHostList;
typedef QList<JobInfoDB*> JobList;

class JobQueueSocket : public QObject
{
  public:
    JobQueueSocket(QString hostname, MythSocket *socket);
   ~JobQueueSocket();

    void UpRef(void);
    bool DownRef(void);

    void SetDisconnected(void) { m_disconnected = true; }
    bool IsDisconnected(void)  { return m_disconnected; }

    MythSocket *getSocket(void) const { return m_socket; }
    QString getHostname(void)   const { return m_hostname; }

    bool AddJob(JobInfoDB *job);
    void DeleteJob(JobInfoDB *job);

    JobList *getAssignedJobs(void);
    int getAssignedJobCount(void);
    
  private:
    QString         m_hostname;
    MythSocket     *m_socket;
    bool            m_disconnected;

    QReadWriteLock  m_jobLock;
    JobList         m_jobList;

    QMutex          m_refLock;
    int             m_refCount;
};

class JobScheduler : public SocketRequestHandler
{
  public:
    JobScheduler(void);
   ~JobScheduler() {};

    bool HandleAnnounce(MythSocket *socket, QStringList &commands,
                        QStringList &slist);
    bool HandleQuery(MythSocket *socket, QStringList &commands,
                     QStringList &slist);
    void connectionClosed(MythSocket *socket);

    QString GetHandlerName(void) { return "JOBMANAGER"; }

    bool             RestartScheduler(void);
    JobHostList     *GetConnectedQueues(void);
    JobQueueSocket  *GetQueueByHostname(QString hostname);

    JobList     *GetQueuedJobs(void);
    JobList     *GetJobByProgram(uint chanid, QDateTime starttime);
    JobList     *GetJobByProgram(ProgramInfo *pginfo);

    JobInfoDB   *GetJobByID(int jobid);
    JobInfoDB   *GetJobByProgram(uint chanid, QDateTime starttime, int cmdid);
    JobInfoDB   *GetJobByProgram(ProgramInfo *pginfo, int cmdid);

    QList<JobCommandDB*> GetCommandList(void);
    JobCommandDB        *GetCommand(int cmdid);
//    QList<JobHostDB*>    *GetHostList(void);
//    QList<JobHostDB*>    *GetHostList(int cmdid);

    void RefreshFromDB(void);

    friend class JobInfoDB;
    friend class JobCommandDB;

  protected:
    void AddCommand(JobCommandDB *cmd);
    void DeleteCommand(JobCommandDB *cmd);
    void AddJob(JobInfoDB *job);
    void DeleteJob(JobInfoDB *job);

  private:
    bool HandleGetJobList(MythSocket *socket);
    bool HandleGetConnectedQueues(MythSocket *socket);
    bool HandleRunScheduler(MythSocket *socket);

    bool HandleQueueJob(MythSocket *socket, JobInfoDB &tmpjob);
    bool HandleGetInfo(MythSocket *socket, QStringList &commands);
    bool HandleSendInfo(MythSocket *socket, JobInfoDB &tmpjob, JobInfoDB *job);
    bool HandlePauseJob(MythSocket *socket, JobInfoDB *job);
    bool HandleResumeJob(MythSocket *socket, JobInfoDB *job);
    bool HandleStopJob(MythSocket *socket, JobInfoDB *job);
    bool HandleRestartJob(MythSocket *socket, JobInfoDB *job);

    bool HandleGetCommand(MythSocket *socket, QStringList &commands);
    bool HandleGetCommands(MythSocket *socket);
    bool HandleSendCommand(MythSocket *socket, JobCommandDB &tmpcmd, JobCommandDB *cmd);
    bool HandleCreateCommand(MythSocket *socket, JobCommandDB &tmpcmd);
    bool HandleDeleteCommand(MythSocket *socket, JobCommandDB *cmd);

    bool HandleGetHost(MythSocket *socket, QStringList &commands);
    bool HandleGetHosts(MythSocket *socket, QStringList &commands);
    bool HandleSendHost(MythSocket *socket, JobHostDB &tmphost, JobHostDB *host);
    bool HandleCreateHost(MythSocket *socket, JobHostDB &tmphost);
    bool HandleDeleteHost(MythSocket *socket, JobHostDB *host);
    


    JobCommandMap   m_cmdMap;
    QReadWriteLock  m_cmdLock;

    JobList         m_jobList;
    QReadWriteLock  m_jobLock;

    JobHostMap      m_hostMap;
    QReadWriteLock  m_hostLock;
};

class JobSchedulerPrivate : public QThread
{
  public:
    JobSchedulerPrivate(JobScheduler *parent);
   ~JobSchedulerPrivate();

    void run(void);

  public slots:
    void wake(void);
    void stop(void);

  protected:
    virtual void DoJobScheduling(void);
    JobScheduler   *m_parent;

  private:
    bool            m_termthread;
    QTimer         *m_timer;

    QMutex          m_lock;
    QWaitCondition  m_waitCond;
};

class JobSchedulerCF : public JobSchedulerPrivate
{
  public:
    JobSchedulerCF(JobScheduler *parent) :
        JobSchedulerPrivate(parent) {}
  protected:
    void DoJobScheduling(void);
};

#endif
