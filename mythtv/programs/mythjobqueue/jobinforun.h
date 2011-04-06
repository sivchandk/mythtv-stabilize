#ifndef _JOBINFORUN_H_
#define _JOBINFORUN_H_

using namespace std;

#include <QString>

#include "mythsystem.h"
#include "jobinfo.h"


class JobInfoRun : public JobInfo
{
  public:
    JobInfoRun(int id) :
        JobInfo(id) {};
    JobInfoRun(const JobInfo &other) :
        JobInfo(other) {};
    JobInfoRun(uint chanid, QDateTime &starttime, int jobType) :
        JobInfo(chanid, starttime, jobType) {};
    JobInfoRun(ProgramInfo &pginfo, int jobType) :
        JobInfo(pginfo, jobType) {};
    JobInfoRun(int jobType, uint chanid, const QDateTime &starttime,
               QString args, QString comment, QString host,
               int flags, int status, QDateTime schedruntime) :
        JobInfo(jobType, chanid, starttime, args, comment, host, flags, 
                status, schedruntime) {};
    JobInfoRun(QStringList::const_iterator &it,
               QStringList::const_iterator end) :
        JobInfo(it, end) {};
    JobInfoRun(const QStringList &slist) :
        JobInfo(slist) {};

    virtual bool Start(void);
    virtual bool Stop(void);
    virtual bool Pause(void);
    virtual bool Resume(void);
    virtual bool Restart(void);
  protected slots:
//    void started(void);

    void finished(void);
    void finishedts(void);
    void finishedcf(void);

    void error(uint status);
    void errorts(uint status);
    void errorcf(uint status);

  private:
    void ConnectMS(void);
    void DisconnectMS(void);

    MythSystem *m_process;

    QString m_command;

    QString GetJobCommand(void);
};
#endif
