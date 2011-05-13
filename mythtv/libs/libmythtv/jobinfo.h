#ifndef JOBINFO_H
#define JOBINFO_H

using namespace std;

// Qt
#include <QMap>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QReadWriteLock>

// MythTV
#include "mythsocket.h"
#include "mythcorecontext.h"
#include "programinfo.h"
#include "mythtvexp.h"
#include "referencecounter.h"

// Strings are used by JobQueue::StatusText()
#define JOBSTATUS_MAP(F) \
    F(JOB_UNKNOWN,      0x0000, QObject::tr("Unknown")) \
    F(JOB_QUEUED,       0x0001, QObject::tr("Queued")) \
    F(JOB_PENDING,      0x0002, QObject::tr("Pending")) \
    F(JOB_STARTING,     0x0004, QObject::tr("Starting")) \
    F(JOB_RUNNING,      0x0008, QObject::tr("Running")) \
    F(JOB_STOPPING,     0x0010, QObject::tr("Stopping")) \
    F(JOB_PAUSED,       0x0020, QObject::tr("Paused")) \
    F(JOB_RETRY,        0x0040, QObject::tr("Retrying")) \
    F(JOB_ERRORING,     0x0080, QObject::tr("Erroring")) \
    F(JOB_ABORTING,     0x0100, QObject::tr("Aborting")) \
    /* \
     * JOB_DONE is a mask to indicate the job is done no matter what the \
     * status \
     */ \
    F(JOB_DONE,         0x8000, QObject::tr("Done (Invalid status!)")) \
    F(JOB_FINISHED,     0x8200, QObject::tr("Finished")) \
    F(JOB_ABORTED,      0x8400, QObject::tr("Aborted")) \
    F(JOB_ERRORED,      0x8800, QObject::tr("Errored")) \
    F(JOB_CANCELLED,    0x9000, QObject::tr("Cancelled")) \

enum JobStatus {
#define JOBSTATUS_ENUM(A,B,C)   A = B ,
    JOBSTATUS_MAP(JOBSTATUS_ENUM)
};

#define NUMJOBINFOLINES 13

class JobBase : public ReferenceCounter
{
  public:
    JobBase(void);
    JobBase(const JobBase &other);
    JobBase(QStringList::const_iterator &it,
            QStringList::const_iterator end);
    JobBase(const QStringList &slist);
   ~JobBase() {};

    JobBase &operator=(const JobBase &other);

    virtual void clone(const JobBase &other) {}
    virtual void clear(void) {}
    virtual bool isStored(void) const { return m_stored; }
            void setStored(bool b)    { m_stored = b; }

    virtual bool SendExpectingInfo(QStringList &strlist, bool clearinfo);
    virtual bool ToStringList(QStringList &strlist) const { return false; }

  protected:
    bool m_stored;

    virtual bool FromStringList(const QStringList &slist);
    virtual bool FromStringList(QStringList::const_iterator &it,
                        QStringList::const_iterator listend) { return false; }
};


class JobInfo;
class JobCommand;

class MTV_PUBLIC JobHost : public JobBase
{
  public:
    JobHost(void);
    JobHost(int cmdid, QString hostname);
    JobHost(int cmdid, QString hostname, QTime runbefore, QTime runafter,
            bool terminate, uint idlemax, uint cpumax, bool create=true);

    JobHost(const JobHost &other) : JobBase(other) {};
    JobHost(QStringList::const_iterator &it,
            QStringList::const_iterator end) : JobBase(it, end) {};
    JobHost(const QStringList &slist) : JobBase(slist) {};

    virtual void clone(const JobHost &other);
    void clear(void);

    // local information retrieval
    int         getCmdID(void)      const { return m_cmdid; }
    QString     getHostname(void)   const { return m_hostname; }
    QTime       getRunBefore(void)  const { return m_runbefore; }
    QTime       getRunAfter(void)   const { return m_runafter; }
    bool        getTerminate(void)  const { return m_terminate; }
    uint        getIdleMax(void)    const { return m_idlemax; }
    uint        getCPUMax(void)     const { return m_cpumax; }

    // local information storage
    void        setRunBefore(QTime time)    { m_runbefore = time; }
    void        setRunAfter(QTime time)     { m_runafter = time; }
    void        setTerminate(bool b=true)   { m_terminate = b; }
    void        setIdleMax(uint max)        { m_idlemax = max; }
    void        setCPUMax(uint max)         { m_cpumax = max; }

    // remote information retrieval
    virtual bool QueryObject(void);
    virtual bool QueryObject(int cmdid, QString hostname);
    
    // remote information storage
    virtual bool SaveObject(void);
    virtual bool Create(void);
    virtual bool Delete(void);

    bool        ToStringList(QStringList &slist) const;
    JobCommand *getJobCommand(void);

  protected:
    bool FromStringList(QStringList::const_iterator &it,
                        QStringList::const_iterator listend);

    int         m_cmdid;
    QString     m_hostname;
    QTime       m_runbefore;
    QTime       m_runafter;
    bool        m_terminate;
    uint        m_idlemax;
    uint        m_cpumax;
};

class MTV_PUBLIC JobCommand : public JobBase
{
  public:
    JobCommand(void);
    JobCommand(int cmdid);
    JobCommand(const JobInfo &job);
    JobCommand(QString type, QString name, QString subname, QString shortdesc,
               QString longdesc, QString path, QString args, bool needsfile,
               bool rundefault, bool cpuintense, bool diskintense,
               bool sequence, bool create=true);

    JobCommand(const JobCommand &other) : JobBase(other) {};
    JobCommand(QStringList::const_iterator &it,
               QStringList::const_iterator end) : JobBase(it, end) {};
    JobCommand(const QStringList &slist) : JobBase(slist) {};

    virtual void clone(const JobCommand &other);
    void clear(void);

    // local information retrieval
    int         getCmdID(void)      const { return m_cmdid; }
    QString     getType(void)       const { return m_type; }
    QString     getName(void)       const { return m_name; }
    QString     getSubName(void)    const { return m_subname; }
    QString     getShortDesc(void)  const { return m_shortdesc; }
    QString     getLongDesc(void)   const { return m_longdesc; }
    QString     getPath(void)       const { return m_path; }
    QString     getArgs(void)       const { return m_args; }
    bool        doesNeedFile(void)  const { return m_needsfile; }
    bool        isDefault(void)     const { return m_rundefault; }
    bool        isCPUIntense(void)  const { return m_cpuintense; }
    bool        isDiskIntense(void) const { return m_diskintense; }
    bool        isSequence(void)    const { return m_sequence; }

    // local information storage
    void        setType(QString type)       { m_type = type; }
    void        setName(QString name)       { m_name = name; }
    void        setSubName(QString name)    { m_subname = name; }
    void        setShortDesc(QString desc)  { m_shortdesc = desc; }
    void        setLongDesc(QString desc)   { m_longdesc = desc; }
    void        setPath(QString path)       { m_path = path; }
    void        setArgs(QString args)       { m_args = args; }
    void        setNeedsFile(bool b=true)   { m_needsfile = b; }
    void        setDefault(bool b=true)     { m_rundefault = b; }
    void        setCPUIntense(bool b=true)  { m_cpuintense = b; }
    void        setDiskIntense(bool b=true) { m_diskintense = b; }
    void        setSequence(bool b=true)    { m_sequence = b; }

    // remote information retrieval
    virtual bool QueryObject(void);
    virtual bool QueryObject(int cmdid);
    // not going to make easy retrival for individual elements
    // for now, since this object should be largely static

    // remote information storage
    virtual bool SaveObject(void);
    virtual bool Create(void);
    virtual bool Delete(void);
    // not going to make easy storage for individual elements
    // for now, since this object should be largely static

    virtual QList<JobHost *> GetEnabledHosts(void);
    virtual QMap<uint, bool> GetAutorunRecordings(void);

//    QList<JobInfo *> GetJobs(uint status=JOB_QUEUED|JOB_RUNNING|JOB_PAUSED);
    JobInfo *QueueRecordingJob(const ProgramInfo &pginfo);
    JobInfo *NewJob(void);

    bool ToStringList(QStringList &slist) const;

  protected:
    bool FromStringList(QStringList::const_iterator &it,
                        QStringList::const_iterator listend);

    int         m_cmdid;
    QString     m_type;
    QString     m_name;
    QString     m_subname;
    QString     m_shortdesc;
    QString     m_longdesc;
    QString     m_path;
    QString     m_args;
    bool        m_needsfile;
    bool        m_rundefault;
    bool        m_cpuintense;
    bool        m_diskintense;
    bool        m_sequence;

    QMap<QString, JobHost*> m_hostMap;
    QReadWriteLock          m_hostLock;

    QMap<uint, bool> m_recordMap;
    QReadWriteLock   m_recordLock;
};

class MTV_PUBLIC JobInfo : public JobBase
{
  public:
    JobInfo(void);
    JobInfo(int id);
    JobInfo(uint chanid, QDateTime &starttime, int cmdid);
    JobInfo(const ProgramInfo &pginfo, int cmdid);
    JobInfo(int cmdid, uint chanid, const QDateTime &starttime,
            int status, QString hostname, QDateTime schedruntime,
            bool create=true);

    JobInfo(const JobInfo &other) : JobBase(other) {};
    JobInfo(QStringList::const_iterator &it,
            QStringList::const_iterator end) : JobBase(it, end) {};
    JobInfo(const QStringList &slist) : JobBase(slist) {};

   ~JobInfo(void);

    virtual void clone(const JobInfo &other);
    void clear(void);

    // local information gets
    int         getJobID(void)        const { return m_jobid; }
    int         getCmdID(void)        const { return m_cmdid; }
    uint        getChanID(void)       const { return m_chanid; }
    QDateTime   getStartTime(void)    const { return m_starttime; }
    int         getStatus(void)       const { return m_status; }
    QString     getComment(void)      const { return m_comment; }
    QDateTime   getStatusTime(void)   const { return m_statustime; }
    QString     getHostname(void)     const { return m_hostname; }
    QDateTime   getSchedRunTime(void) const { return m_schedruntime; }
    uint        getCPUTime(void)      const { return m_cputime; }
    uint        getDuration(void)     const { return m_duration; }

    JobCommand  *getCommand(void);
    ProgramInfo *getProgramInfo(void);

    // local information saves
    void        setStatus(int status)       { m_status = status;
                                              setStatusTime(); }
    void        setStatus(int status, QString comment);
    void        setComment(QString comment) { m_comment = comment;
                                              setStatusTime(); }
    void        setSchedRunTime(QDateTime runtime)
                                            { m_schedruntime = runtime; }
    void        setStatusTime(void)         { m_statustime = 
                                                QDateTime::currentDateTime(); }
    void        setHostname(QString hostname)
                                            { m_hostname = hostname; }
    void        setCPUTime(uint cputime)    { m_cputime = cputime; }
    void        setDuration(uint duration)  { m_duration = duration; }

    void        setCmdID(int cmdid);
    void        setProgram(int chanid, QDateTime starttime);
    void        setProgram(ProgramInfo *pginfo);

    virtual bool QueryObject(void) { return QueryObject(m_jobid); }
    virtual bool QueryObject(int jobid);
    virtual bool QueryObject(uint chanid, const QDateTime &starttime, int cmdid);

    virtual bool SaveObject(void);
    virtual bool Queue(void);

    virtual bool Pause(void);
    virtual bool Resume(void);
    virtual bool Stop(void);
    virtual bool Restart(void);

    QString     GetStatusText(void);
    bool        IsRecording(void);

    // remote information gets
    int         queryStatus(void)           { QueryObject();
                                              return getStatus(); }
    QString     queryComment(void)          { QueryObject();
                                              return getComment(); }
    QDateTime   queryStatusTime(void)       { QueryObject();
                                              return getStatusTime(); }
    QDateTime   querySchedRunTime(void)     { QueryObject();
                                              return getSchedRunTime(); }
    QString     queryHostname(void)         { QueryObject();
                                              return getHostname(); }
    uint        queryCPUTime(void)          { QueryObject();
                                              return getCPUTime(); }
    uint        queryDuration(void)         { QueryObject();
                                              return getDuration(); }

    // remote information saves
    void        saveStatus(int status)      { setStatus(status); SaveObject(); }
    void        saveStatus(int status, QString comment);
    void        saveComment(QString comment){ setComment(comment);
                                              SaveObject(); }
    void        saveSchedRunTime(QDateTime runtime)
                                            { setSchedRunTime(runtime);
                                              SaveObject(); }
    void        saveHostname(QString hostname)
                                            { setHostname(hostname);
                                              SaveObject(); }
    void        saveCPUTime(uint cputime)   { setCPUTime(cputime);
                                              SaveObject(); }
    void        saveDuration(uint duration) { setDuration(duration);
                                              SaveObject(); }

    bool        ToStringList(QStringList &slist) const;

    void        PrintToLog(void);

  protected:
    int         m_jobid;
    int         m_cmdid;
    uint        m_chanid;
    QDateTime   m_starttime;
    uint        m_status;
    QString     m_comment;
    QDateTime   m_statustime;
    QString     m_hostname;
    QDateTime   m_schedruntime;
    uint        m_cputime;
    uint        m_duration;

    JobCommand  *m_command;
    ProgramInfo *m_pgInfo;

    bool        FromStringList(QStringList::const_iterator &it,
                               QStringList::const_iterator listend);
};

#endif
