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

// MythTV
#include "mythsocket.h"
#include "mythcorecontext.h"
#include "programinfo.h"
#include "mythtvexp.h"

// Strings are used by JobQueue::StatusText()
#define JOBSTATUS_MAP(F) \
    F(JOB_UNKNOWN,      0x0000, QObject::tr("Unknown")) \
    F(JOB_QUEUED,       0x0001, QObject::tr("Queued")) \
    F(JOB_PENDING,      0x0002, QObject::tr("Pending")) \
    F(JOB_STARTING,     0x0003, QObject::tr("Starting")) \
    F(JOB_RUNNING,      0x0004, QObject::tr("Running")) \
    F(JOB_STOPPING,     0x0005, QObject::tr("Stopping")) \
    F(JOB_PAUSED,       0x0006, QObject::tr("Paused")) \
    F(JOB_RETRY,        0x0007, QObject::tr("Retrying")) \
    F(JOB_ERRORING,     0x0008, QObject::tr("Erroring")) \
    F(JOB_ABORTING,     0x0009, QObject::tr("Aborting")) \
    /* \
     * JOB_DONE is a mask to indicate the job is done no matter what the \
     * status \
     */ \
    F(JOB_DONE,         0x0100, QObject::tr("Done (Invalid status!)")) \
    F(JOB_FINISHED,     0x0110, QObject::tr("Finished")) \
    F(JOB_ABORTED,      0x0120, QObject::tr("Aborted")) \
    F(JOB_ERRORED,      0x0130, QObject::tr("Errored")) \
    F(JOB_CANCELLED,    0x0140, QObject::tr("Cancelled")) \

enum JobStatus {
#define JOBSTATUS_ENUM(A,B,C)   A = B ,
    JOBSTATUS_MAP(JOBSTATUS_ENUM)
};

enum JobCmds {
    JOB_RUN          = 0x0000,
    JOB_PAUSE        = 0x0001,
    JOB_RESUME       = 0x0002,
    JOB_STOP         = 0x0004,
    JOB_RESTART      = 0x0008
};

enum JobFlags {
    JOB_NO_FLAGS     = 0x0000,
    JOB_USE_CUTLIST  = 0x0001,
    JOB_LIVE_REC     = 0x0002,
    JOB_EXTERNAL     = 0x0004
};

enum JobLists {
    JOB_LIST_ALL      = 0x0001,
    JOB_LIST_DONE     = 0x0002,
    JOB_LIST_NOT_DONE = 0x0004,
    JOB_LIST_ERROR    = 0x0008,
    JOB_LIST_RECENT   = 0x0010
};

enum JobTypes {
    JOB_NONE         = 0x0000,

    JOB_SYSTEMJOB    = 0x00ff,
    JOB_TRANSCODE    = 0x0001,
    JOB_COMMFLAG     = 0x0002,

    JOB_USERJOB      = 0xff00,
    JOB_USERJOB1     = 0x0100,
    JOB_USERJOB2     = 0x0200,
    JOB_USERJOB3     = 0x0400,
    JOB_USERJOB4     = 0x0800
};

#define NUMJOBINFOLINES 13

class MTV_PUBLIC JobInfo : public QObject
{
    Q_OBJECT
  public:
    JobInfo();
    JobInfo(int id);
    JobInfo(const JobInfo &other);
    JobInfo(uint chanid, QDateTime &starttime, int jobType);
    JobInfo(const ProgramInfo &pginfo, int jobType);
    JobInfo(int jobType, uint chanid, const QDateTime &starttime,
            QString args, QString comment, QString host,
            int flags, int status, QDateTime schedruntime);
    JobInfo(QStringList::const_iterator &it,
            QStringList::const_iterator end);
    JobInfo(const QStringList &slist);


   ~JobInfo(void);

    JobInfo &operator=(const JobInfo &other);
    virtual void clone(const JobInfo &other);
    void clear(void);

    bool isValid(void)                const { return (m_jobid != -1); }

    // local information gets
    int         getJobID(void)        const { return m_jobid; }
    uint        getChanID(void)       const { return m_chanid; }
    QDateTime   getStartTime(void)    const { return m_starttime; }
    QDateTime   getInsertTime(void)   const { return m_inserttime; }
    int         getJobType(void)      const { return m_jobType; }
    int         getCmds(void)         const { return m_cmds; }
    int         getFlags(void)        const { return m_flags; }
    int         getStatus(void)       const { return m_status; }
    QDateTime   getStatusTime(void)   const { return m_statustime; }
    QString     getHostname(void)     const { return m_hostname; }
    QString     getArgs(void)         const { return m_args; }
    QString     getComment(void)      const { return m_comment; }
    QDateTime   getSchedRunTime(void) const { return m_schedruntime; }

    // local information saves
    void        setCmds(int cmds)           { m_cmds = cmds; }
    void        setFlags(int flags)         { m_flags = flags; }
    void        setStatus(int status)       { m_status = status;
                                              setStatusTime(); }
    void        setStatus(int status, QString comment);
    void        setComment(QString comment) { m_comment = comment;
                                              setStatusTime(); }
    void        setArgs(QString args)       { m_args = args; }
    void        setSchedRunTime(QDateTime runtime)
                                            { m_schedruntime = runtime; }
    void        setStatusTime(void)         { m_statustime = 
                                                QDateTime::currentDateTime(); }
    void        setHostname(QString hostname)
                                            { m_hostname = hostname; }

    virtual bool QueryObject(void);
    virtual bool SaveObject(void);
    virtual bool Queue(void);

    virtual bool Pause(void);
    virtual bool Resume(void);
    virtual bool Stop(void);
    virtual bool Restart(void);

    int         GetUserJobIndex(void);
    QString     GetJobDescription(void);
    QString     GetStatusText(void);
    ProgramInfo *GetPGInfo(void);
    bool        IsRecording(void);

    // remote information gets
    int         queryCmds(void)             { QueryObject(); return getCmds(); }
    int         queryFlags(void)            { QueryObject();
                                              return getFlags(); }
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
    QString     queryArgs(void)             { QueryObject(); return getArgs(); }

    // remote information saves
    void        saveCmds(int cmds)          { setCmds(cmds); SaveObject(); }
    void        saveFlags(int flags)        { setFlags(flags); SaveObject(); }
    void        saveStatus(int status)      { setStatus(status); SaveObject(); }
    void        saveStatus(int status, QString comment);
    void        saveComment(QString comment){ setComment(comment);
                                              SaveObject(); }
    void        saveArgs(QString args)      { setArgs(args); SaveObject(); }
    void        saveSchedRunTime(QDateTime runtime)
                                            { setSchedRunTime(runtime);
                                              SaveObject(); }
    void        saveHostname(QString hostname)
                                            { setHostname(hostname);
                                              SaveObject(); }

    bool        SendExpectingInfo(QStringList &sl, bool clearinfo);
    bool        ToStringList(QStringList &slist) const;
    bool        QueryObject(int chanid, QDateTime starttime, int jobType);

    void        UpRef(void);
    bool        DownRef(void);

  protected:
    int         m_jobid;
    uint        m_chanid;
    QDateTime   m_starttime;
    QDateTime   m_inserttime;
    int         m_jobType;
    int         m_cmds;
    int         m_flags;
    int         m_status;
    QDateTime   m_statustime;
    QString     m_hostname;
    QString     m_args;
    QString     m_comment;
    QDateTime   m_schedruntime;

    bool        FromStringList(const QStringList &slist);
    bool        FromStringList(QStringList::const_iterator &it,
                               QStringList::const_iterator listend);

    int         m_userJobIndex;
    ProgramInfo *m_pgInfo;

    int         m_refcount;
    QMutex      m_reflock;
};

#endif
