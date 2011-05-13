#ifndef JOBINFODB_H_
#define JOBINFODB_H_

using namespace std;

#include "mythsocket.h"
#include "jobinfo.h"
#include "mythdb.h"

class JobScheduler;

class JobHostDB : public JobHost
{
  public:
    JobHostDB(void) : JobHost() {};
    JobHostDB(int cmdid, QString hostname);
    JobHostDB(int cmdid, QString hostname, QTime runbefore, QTime runafter,
              bool terminate, uint idlemax, uint cpumax);
    JobHostDB(const JobHostDB &other) : JobHost(other) {};
    JobHostDB(QStringList::const_iterator &it,
              QStringList::const_iterator end) : JobHost(it, end) {};
    JobHostDB(const QStringList &slist) : JobHost(slist) {};
    JobHostDB(MSqlQuery &query) : JobHost() { fromQuery(query); }

    bool QueryObject(void) { return QueryObject(m_cmdid, m_hostname); }
    bool QueryObject(int cmdid, QString hostname);

    bool Create(void);
    bool SaveObject(void);
    bool Delete(void);
    static bool Delete(int cmdid, QString hostname);

  private:
    bool fromQuery(MSqlQuery &query);
};

class JobCommandDB : public JobCommand
{
  public:
    JobCommandDB(void) : JobCommand() {};
    JobCommandDB(int cmdid);
    JobCommandDB(const JobInfo &job) : JobCommand(job) {};
    JobCommandDB(QString type, QString name, QString subname,
                 QString shortdesc, QString longdesc, QString path,
                 QString args, bool needsfile, bool rundefault,
                 bool cpuintense, bool diskintense, bool sequence);
    JobCommandDB(const JobCommandDB &other) : JobCommand(other) {};
    JobCommandDB(QStringList::const_iterator &it,
                 QStringList::const_iterator end) : JobCommand(it, end) {};
    JobCommandDB(const QStringList &slist) : JobCommand(slist) {};
    JobCommandDB(MSqlQuery &query) : JobCommand() { fromQuery(query); }

    bool QueryObject(void) { return QueryObject(m_cmdid); }
    bool QueryObject(int cmdid);

    bool Create(void);
    bool SaveObject(void);
    bool Delete(void);
    static bool Delete(int cmdid);

    QList<JobHost*> GetEnabledHosts(void);
    JobHost        *GetEnabledHost(QString hostname);
    bool            ContainsHost(QString hostname);

    friend class JobScheduler;
    friend class JobHostDB;
    friend class JobHost;

  protected:
    void AddHost(JobHostDB *host);
    void DeleteHost(JobHostDB *host);

  private:
    bool fromQuery(MSqlQuery &query);
};

class JobInfoDB : public JobInfo
{
  public:
    JobInfoDB(void) : JobInfo() {};
    JobInfoDB(int id);
    JobInfoDB(const JobInfoDB &other) : JobInfo(other) {};
    JobInfoDB(uint chanid, QDateTime &starttime, int cmdid);
    JobInfoDB(ProgramInfo &pginfo, int cmdid);
    JobInfoDB(int cmdid, uint chanid, const QDateTime &starttime,
              int status, QString hostname, QDateTime schedruntime);
    JobInfoDB(QStringList::const_iterator &it,
               QStringList::const_iterator end) :
        JobInfo(it, end) {};
    JobInfoDB(const QStringList &slist) :
        JobInfo(slist) {};
    JobInfoDB(MSqlQuery &query) : JobInfo() { fromQuery(query); }

    bool QueryObject(int jobid);
    bool QueryObject(uint chanid, QDateTime starttime, int cmdid);

    bool SaveObject(void);
    bool Queue(void);
    bool Delete(void);
    static bool Delete(int jobID);

    MythSocket *GetHost(void) { return m_hostSocket; }
    void        SetHost(MythSocket *socket) { m_hostSocket = socket; }

    bool Run(MythSocket *socket);
    bool Pause(void);
    bool Resume(void);
    bool Stop(void);
    bool Restart(void);

    friend class JobScheduler;

  private:
    bool fromQuery(MSqlQuery &query);

    MythSocket *m_hostSocket;
};
#endif
