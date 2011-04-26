#ifndef JOBINFODB_H_
#define JOBINFODB_H_

using namespace std;

#include "mythsocket.h"
#include "jobinfo.h"

class JobInfoDB : public JobInfo
{
  public:
    JobInfoDB(int id);
    JobInfoDB(const JobInfoDB &other);
    JobInfoDB(uint chanid, QDateTime &starttime, int jobType);
    JobInfoDB(ProgramInfo &pginfo, int jobType);
    JobInfoDB(int jobType, uint chanid, const QDateTime &starttime,
               QString args, QString comment, QString host,
               int flags, int status, QDateTime schedruntime) :
        JobInfo(jobType, chanid, starttime, args, comment, host, flags, 
                status, schedruntime) {};
    JobInfoDB(QStringList::const_iterator &it,
               QStringList::const_iterator end) :
        JobInfo(it, end) {};
    JobInfoDB(const QStringList &slist) :
        JobInfo(slist) {};

    virtual bool QueryObject(void);
    virtual bool SaveObject(void);
    virtual bool Queue(void);
    static  bool Delete(int jobID);
            bool Delete(void);

    MythSocket *GetHost(void) { return m_hostSocket; }
    void        SetHost(MythSocket *socket) { m_hostSocket = socket; }

    bool Run(MythSocket *socket);
    bool Pause(void);
    bool Resume(void);
    bool Stop(void);
    bool Restart(void);
  private:
    MythSocket *m_hostSocket;
};
#endif
