#ifndef _JOBINFORUN_H_
#define _JOBINFORUN_H_

using namespace std;

#include <QString>

#include "mythsystem.h"
#include "jobinfo.h"


class JobInfoRun : public QObject, public JobInfo
{
    Q_OBJECT
  public:
    JobInfoRun(int id) :
        JobInfo(id), m_process(NULL) {};
    JobInfoRun(const JobInfoRun &other) :
        JobInfo(other), m_process(NULL) {};
    JobInfoRun(uint chanid, QDateTime &starttime, int cmdid) :
        JobInfo(chanid, starttime, cmdid), m_process(NULL) {};
    JobInfoRun(ProgramInfo &pginfo, int cmdid) :
        JobInfo(pginfo, cmdid), m_process(NULL) {};
    JobInfoRun(int cmdid, uint chanid, const QDateTime &starttime,
               int status, QString hostname, QDateTime schedruntime) :
        JobInfo(cmdid, chanid, starttime, status, hostname, schedruntime),
        m_process(NULL) {};
    JobInfoRun(QStringList::const_iterator &it,
               QStringList::const_iterator end) :
        JobInfo(it, end), m_process(NULL) {};
    JobInfoRun(const QStringList &slist) :
        JobInfo(slist), m_process(NULL) {};

    virtual bool Start(void);
    virtual bool Stop(void);
    virtual bool Pause(void);
    virtual bool Resume(void);
    virtual bool Restart(void);
  protected slots:
//    void started(void);

    void finished(void);
    void error(uint status);

  private:
    void ConnectMS(void);
    void DisconnectMS(void);

    MythSystem *m_process;

    QString m_cmdstr;

    QString GetJobCommand(void);
};
#endif
