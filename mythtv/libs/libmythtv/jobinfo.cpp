
#include <unistd.h>
#include <iostream>
#include <cstdlib>
using namespace std;

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include "mythconfig.h"
#include "exitcodes.h"
#include "jobinfo.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "mythverbose.h"

#define LOC     QString("JobInfo: ")
#define LOC_ERR QString("JobInfo Error: ")


// for serialization
#define INT_TO_LIST(x)       do { list << QString::number(x); } while (0)
#define BOOL_TO_LIST(x)      do { list << QString((x)?"1":"0"); } while (0)
#define DATETIME_TO_LIST(x)  INT_TO_LIST((x).toTime_t())
#define TIME_TO_LIST(x)      INT_TO_LIST((x).hour()*3600 + (x).minute()+60 + (x).second())
#define STR_TO_LIST(x)       do { list << (x); } while (0)

// for deserialization
#define NEXT_STR()        do { if (it == listend)                    \
                               {                                     \
                                   VERBOSE(VB_IMPORTANT, listerror); \
                                   clear();                          \
                                   return false;                     \
                               }                                     \
                               ts = *it++; } while (0)
#define INT_FROM_LIST(x)     do { NEXT_STR(); (x) = ts.toLongLong(); } while (0)
#define BOOL_FROM_LIST(x)    do { NEXT_STR(); (x) = (ts.toInt() == 1); } while (0)
#define ENUM_FROM_LIST(x, y) do { NEXT_STR(); (x) = ((y)ts.toInt()); } while (0)
#define DATETIME_FROM_LIST(x) \
    do { NEXT_STR(); (x).setTime_t(ts.toUInt()); } while (0)
#define TIME_FROM_LIST(x) \
    do { NEXT_STR(); int tsi = ts.toInt(); (x) = QTime(tsi/3600, (tsi%3600)/60, tsi%60); } while (0)
#define STR_FROM_LIST(x)     do { NEXT_STR(); (x) = ts; } while (0)


/*
 * JobBase - base inheritance class for sending/receiving
 *           jobqueue information across the backend protocol
 */
JobBase::JobBase(void) : ReferenceCounter(), m_stored(false)
{
}

JobBase::JobBase(const JobBase &other) : ReferenceCounter(), m_stored(false)
{
    clone(other);
}

JobBase::JobBase(QStringList::const_iterator &it,
                 QStringList::const_iterator end) :
    ReferenceCounter(), m_stored(false)
{
    if (!FromStringList(it, end))
        clear();
}

JobBase::JobBase(const QStringList &slist) :
    ReferenceCounter(), m_stored(false)
{
    if (!FromStringList(slist))
        clear();
}

JobBase &JobBase::operator=(const JobBase &other)
{
    clone(other);
    return *this;
}

bool JobBase::SendExpectingInfo(QStringList &sl, bool clearinfo)
{
    if (!gCoreContext->SendReceiveStringList(sl))
    {
        if (clearinfo)
            clear();
        return false;
    }

    if (sl[0] == "ERROR")
    {
        if (clearinfo)
            clear();
        return false;
    }

    if (!FromStringList(sl))
    {
        if (clearinfo)
            clear();
        return false;
    }

    setStored(true);
    return true;
}

bool JobBase::FromStringList(const QStringList &slist)
{
    QStringList::const_iterator it = slist.constBegin();
    return FromStringList(it, slist.constEnd());
}

/*
 *
 *
 */
JobHost::JobHost(void) : JobBase(),
    m_cmdid(-1), m_hostname(""), m_runbefore(23,59,59), m_runafter(0,0,0),
    m_terminate(0), m_idlemax(1800), m_cpumax(0)
{
}

JobHost::JobHost(int cmdid, QString hostname) : JobBase()
{
    QueryObject(cmdid, hostname);
}

JobHost::JobHost(int cmdid, QString hostname, QTime runbefore, QTime runafter,
                 bool terminate, uint idlemax, uint cpumax, bool create) :
    JobBase(), m_cmdid(cmdid), m_hostname(hostname), m_runbefore(runbefore),
    m_runafter(runafter), m_terminate(terminate), m_idlemax(idlemax),
    m_cpumax(cpumax)
{
    if (create)
        Create();
}


void JobHost::clone(const JobHost &other)
{
    m_cmdid = other.m_cmdid;
    m_hostname = other.m_hostname;
    m_runbefore = other.m_runbefore;
    m_runafter = other.m_runafter;
    m_terminate = other.m_terminate;
    m_idlemax = other.m_idlemax;
    m_cpumax = other.m_cpumax;

    setStored(other.isStored());
}

void JobHost::clear(void)
{
    m_cmdid = -1;
    m_hostname = "";
    m_runbefore = QTime(23,59,59);
    m_runafter = QTime(0,0,0);
    m_terminate = 0;
    m_idlemax = 1800; 
    m_cpumax = 0;

    setStored(false);
}

bool JobHost::QueryObject(void)
{
    if (!isStored())
    {
        clear();
        return false;
    }
    return QueryObject(m_cmdid, m_hostname);
}

bool JobHost::QueryObject(int cmdid, QString hostname)
{
    QStringList sl(QString("QUERY_JOBQUEUE GET_HOST %1 %2")
                        .arg(cmdid).arg(hostname));
    return SendExpectingInfo(sl, true);
}

bool JobHost::Create(void)
{
    if (isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE ADD_HOST");
    if (!ToStringList(sl))
        return false;

    return SendExpectingInfo(sl, false);
}

bool JobHost::SaveObject(void)
{
    if (!isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE SEND_HOST");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobHost::Delete(void)
{
    if (!isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE DELETE_HOST");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobHost::ToStringList(QStringList &list) const
{
    INT_TO_LIST(m_cmdid);
    STR_TO_LIST(m_hostname);
    TIME_TO_LIST(m_runbefore);
    TIME_TO_LIST(m_runafter);
    INT_TO_LIST(m_terminate);
    INT_TO_LIST(m_idlemax);
    INT_TO_LIST(m_cpumax);

    return true;
}

bool JobHost::FromStringList(QStringList::const_iterator &it,
                             QStringList::const_iterator listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;

    INT_FROM_LIST(m_cmdid);
    STR_FROM_LIST(m_hostname);
    TIME_FROM_LIST(m_runbefore);
    TIME_FROM_LIST(m_runafter);
    INT_FROM_LIST(m_terminate);
    INT_FROM_LIST(m_idlemax);
    INT_FROM_LIST(m_cpumax);

    return true;
}

JobCommand *JobHost::getJobCommand(void)
{
    JobCommand *jcmd = NULL;
    if (isStored())
        jcmd = new JobCommand(m_cmdid);
    return jcmd;
}

/*
 *
 *
 */
JobCommand::JobCommand(void) : JobBase(),
    m_cmdid(-1), m_type(""), m_name(""), m_subname(""),
    m_shortdesc(""), m_longdesc(""), m_path(""), m_args(""),
    m_needsfile(false), m_rundefault(false), m_cpuintense(false),
    m_diskintense(false), m_sequence(false)
{
}

JobCommand::JobCommand(int cmdid) : JobBase()
{
    QueryObject(cmdid);
}

JobCommand::JobCommand(const JobInfo &job) : JobBase()
{
    QueryObject(job.getCmdID());
}

JobCommand::JobCommand(QString type, QString name, QString subname,
                       QString shortdesc, QString longdesc, QString path,
                       QString args, bool needsfile, bool rundefault,
                       bool cpuintense, bool diskintense, bool sequence,
                       bool create) :
    JobBase(), m_type(type), m_name(name), m_subname(subname),
    m_shortdesc(shortdesc), m_longdesc(longdesc), m_path(path), m_args(args),
    m_needsfile(needsfile), m_rundefault(rundefault), m_cpuintense(cpuintense),
    m_diskintense(diskintense), m_sequence(sequence)
{
    if (create)
        Create();
}

void JobCommand::clone(const JobCommand &other)
{
    m_cmdid = other.m_cmdid;
    m_type = other.m_type;
    m_name = other.m_name;
    m_subname = other.m_subname;
    m_shortdesc = other.m_shortdesc;
    m_longdesc = other.m_longdesc;
    m_path = other.m_path;
    m_args = other.m_args;
    m_needsfile = other.m_needsfile;
    m_rundefault = other.m_rundefault;
    m_cpuintense = other.m_cpuintense;
    m_diskintense = other.m_diskintense;
    m_sequence = other.m_sequence;

    setStored(other.isStored());
}

void JobCommand::clear(void)
{
    m_cmdid = -1;
    m_type = "";
    m_name = "";
    m_subname = "";
    m_shortdesc = "";
    m_longdesc = "";
    m_path = "";
    m_args = "";
    m_needsfile = false;
    m_rundefault = false;
    m_cpuintense = false;
    m_diskintense = false;
    m_sequence = false;

    setStored(false);
}

bool JobCommand::QueryObject(void)
{
    if (!isStored())
    {
        clear();
        return false;
    }
    return QueryObject(m_cmdid);
}

bool JobCommand::QueryObject(int cmdid)
{
    QStringList sl(QString("QUERY_JOBQUEUE GET_COMMAND %1")
                        .arg(cmdid));
    return SendExpectingInfo(sl, true);
}

bool JobCommand::Create(void)
{
    if (isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE CREATE_COMMAND");
    if (!ToStringList(sl))
        return false;

    return SendExpectingInfo(sl, true);
}

bool JobCommand::SaveObject(void)
{
    if (!isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE SEND_COMMAND");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobCommand::Delete(void)
{
    if (!isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE DELETE_COMMAND");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobCommand::ToStringList(QStringList &list) const
{
    INT_TO_LIST(m_cmdid);
    STR_TO_LIST(m_type);
    STR_TO_LIST(m_name);
    STR_TO_LIST(m_subname);
    STR_TO_LIST(m_shortdesc);
    STR_TO_LIST(m_longdesc);
    STR_TO_LIST(m_path);
    STR_TO_LIST(m_args);
    BOOL_TO_LIST(m_needsfile);
    BOOL_TO_LIST(m_rundefault);
    BOOL_TO_LIST(m_cpuintense);
    BOOL_TO_LIST(m_diskintense);
    BOOL_TO_LIST(m_sequence);

    return true;
}

bool JobCommand::FromStringList(QStringList::const_iterator &it,
                                QStringList::const_iterator listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;

    INT_FROM_LIST(m_cmdid);
    STR_FROM_LIST(m_type);
    STR_FROM_LIST(m_name);
    STR_FROM_LIST(m_subname);
    STR_FROM_LIST(m_shortdesc);
    STR_FROM_LIST(m_longdesc);
    STR_FROM_LIST(m_path);
    STR_FROM_LIST(m_args);
    BOOL_FROM_LIST(m_needsfile);
    BOOL_FROM_LIST(m_rundefault);
    BOOL_FROM_LIST(m_cpuintense);
    BOOL_FROM_LIST(m_diskintense);
    BOOL_FROM_LIST(m_sequence);

    return true;
}

QList<JobHost *> JobCommand::GetEnabledHosts(void)
{
    QList<JobHost *> jl;
    if (!isStored())
        return jl;

    QStringList sl(QString("QUERY_JOBQUEUE GET_HOSTS %1").arg(m_cmdid));
    if (!gCoreContext->SendReceiveStringList(sl))
        return jl;

    JobHost *host = NULL;
    QStringList::const_iterator it = sl.begin();
    QStringList::const_iterator end = sl.end();

    VERBOSE(VB_JOBQUEUE, QString("JobCommand(%1) is runnable on %2 hosts.")
                .arg(m_cmdid).arg(*it++));

    while (it != end)
    {
        host = new JobHost(it, end);
        if (!host->isStored())
        {
            delete host;
            QList<JobHost *>::iterator it2;
            for (it2 = jl.begin(); it2 != jl.end(); ++it2)
                delete (*it2);
        }
        jl << host;
    }

    return jl;
}

QMap<uint, bool> JobCommand::GetAutorunRecordings(void)
{
    QMap<uint, bool> jr;
    if (!isStored())
        return jr;

    QStringList sl(QString("QUERY_JOBQUEUE GET_AUTORUN %1").arg(m_cmdid));
    if (!gCoreContext->SendReceiveStringList(sl))
        return jr;

    QStringList::const_iterator it = sl.begin();
    VERBOSE(VB_JOBQUEUE, QString("JobCommand(%1) is autorun on %2 rules.")
                .arg(m_cmdid).arg(*it++));
    for(;it != sl.end(); ++it)
        jr.insert((*it++).toInt(), (*it) == "1");

    return jr;
}

/*QList<JobInfo *> JobCommand::GetJobs(uint status)
{

}*/

JobInfo *JobCommand::QueueRecordingJob(const ProgramInfo &pginfo)
{
    JobInfo *ji = new JobInfo(pginfo, m_cmdid);
    if (!ji->isStored())
    {
        delete ji;
        return NULL;
    }
    return ji;
}

JobInfo *JobCommand::NewJob(void)
{
    JobInfo *ji = new JobInfo();
    ji->setCmdID(m_cmdid);
    return ji;
}

/*
 *
 *
 */
JobInfo::JobInfo(void) :
    JobBase(), m_command(NULL), m_pgInfo(NULL)
{
}

JobInfo::JobInfo(int id) :
     JobBase(), m_jobid(id), m_command(NULL), m_pgInfo(NULL)
{
    QueryObject();
}

JobInfo::JobInfo(uint chanid, QDateTime &starttime, int cmdid) :
    JobBase(), m_command(NULL), m_pgInfo(NULL)
{
    QueryObject(chanid, starttime, cmdid);
}

JobInfo::JobInfo(const ProgramInfo &pginfo, int cmdid) :
    JobBase(), m_command(NULL), m_pgInfo(NULL)
{
    QueryObject(pginfo.GetChanID(), pginfo.GetRecordingStartTime(), cmdid);
}

JobInfo::JobInfo(int cmdid, uint chanid, const QDateTime &starttime,
                 int status, QString hostname, QDateTime schedruntime,
                 bool create) :
    JobBase(), m_cmdid(cmdid), m_chanid(chanid), m_starttime(starttime),
    m_status(status), m_hostname(hostname), m_schedruntime(schedruntime),
    m_command(NULL), m_pgInfo(NULL)
{
    if (create)
        Queue();
}

JobInfo::~JobInfo(void)
{
    if (m_command)
        m_command->DownRef();
    if (m_pgInfo)
        delete m_pgInfo;
}

void JobInfo::clone(const JobInfo &other)
{
    m_jobid = other.m_jobid;
    m_cmdid = other.m_cmdid;
    m_chanid = other.m_chanid;
    m_starttime = other.m_starttime;
    m_status = other.m_status;
    m_comment = other.m_comment;
    m_statustime = other.m_statustime;
    m_hostname = other.m_hostname;
    m_schedruntime = other.m_schedruntime;
    m_cputime = other.m_cputime;
    m_duration = other.m_duration;

    setStored(other.isStored());
}

void JobInfo::clear(void)
{
    m_jobid = -1;
    m_cmdid = -1;
    m_chanid = 0;
    m_starttime = QDateTime::currentDateTime();
    m_status = 0;
    m_comment.clear();
    m_statustime = m_starttime;
    m_hostname.clear();
    m_schedruntime = m_starttime;
    m_cputime = 0;
    m_duration = 0;

    setStored(false);
}

bool JobInfo::ToStringList(QStringList &list) const
{
    INT_TO_LIST(m_jobid);
    INT_TO_LIST(m_cmdid);
    INT_TO_LIST(m_chanid);
    DATETIME_TO_LIST(m_starttime);
    INT_TO_LIST(m_status);
    STR_TO_LIST(m_comment);
    DATETIME_TO_LIST(m_statustime);
    STR_TO_LIST(m_hostname);
    DATETIME_TO_LIST(m_schedruntime);
    INT_TO_LIST(m_cputime);
    INT_TO_LIST(m_duration);

    return true;
}

bool JobInfo::FromStringList(QStringList::const_iterator &it,
                             QStringList::const_iterator listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;

    INT_FROM_LIST(m_jobid);
    INT_FROM_LIST(m_cmdid);
    INT_FROM_LIST(m_chanid);
    DATETIME_FROM_LIST(m_starttime);
    INT_FROM_LIST(m_status);
    STR_FROM_LIST(m_comment);
    DATETIME_FROM_LIST(m_statustime);
    STR_FROM_LIST(m_hostname);
    DATETIME_FROM_LIST(m_schedruntime);
    INT_FROM_LIST(m_cputime);
    INT_FROM_LIST(m_duration);

    return true;
}

void JobInfo::setStatus(int status, QString comment)
{
    if (status != getStatus())
    {
        setStatus(status);
        setComment(comment);
        setStatusTime();
    }
}

void JobInfo::saveStatus(int status, QString comment)
{
    if (status != getStatus())
    {
        setStatus(status);
        setComment(comment);
        setStatusTime();
        SaveObject();
    }
}

void JobInfo::setCmdID(int cmdid)
{
    // only allow this operation if job is not stored in the database
    if (!isStored())
        m_cmdid = cmdid;
}

void JobInfo::setProgram(int chanid, QDateTime starttime)
{
    // only allow this operation if job is not stored in the database
    if (!isStored())
    {
        m_chanid = chanid;
        m_starttime = starttime;
    }
}

void JobInfo::setProgram(ProgramInfo *pginfo)
{
    if (!isStored())
    {
        setProgram(pginfo->GetChanID(), pginfo->GetRecordingStartTime());
        if (m_pgInfo)
            delete m_pgInfo;
        m_pgInfo = pginfo;
    }
}

bool JobInfo::QueryObject(int jobid)
{
    QStringList sl(QString("QUERY_JOBQUEUE GET_INFO %1").arg(jobid));
    return SendExpectingInfo(sl, true);
}

bool JobInfo::QueryObject(uint chanid, const QDateTime &starttime, int cmdid)
{
    QStringList sl(QString("QUERY_JOBQUEUE GET_INFO %1 %2 %3")
                        .arg(chanid).arg(starttime.toTime_t()).arg(cmdid));
    return SendExpectingInfo(sl, true);
}

bool JobInfo::SaveObject(void)
{
    if (!isStored())
        return false;

    QStringList sl("QUERY_JOBQUEUE SEND_INFO");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobInfo::Queue(void)
{
    QStringList sl("QUERY_JOBQUEUE QUEUE_JOB");
    if (!ToStringList(sl))
        return false;
    return SendExpectingInfo(sl, false);
}

bool JobInfo::Pause(void)
{
    QStringList sl("QUERY_JOBQUEUE PAUSE_JOB");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobInfo::Resume(void)
{
    QStringList sl("QUERY_JOBQUEUE RESUME_JOB");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobInfo::Stop(void)
{
    QStringList sl("QUERY_JOBQUEUE STOP_JOB");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

bool JobInfo::Restart(void)
{
    QStringList sl("QUERY_JOBQUEUE RESTART_JOB");
    if (!ToStringList(sl))
        return false;

    if (!gCoreContext->SendReceiveStringList(sl))
        return false;

    if (sl[0] == "ERROR")
        return false;

    return true;
}

QString JobInfo::GetStatusText(void)
{
    switch (m_status)
    {
#define JOBSTATUS_STATUSTEXT(A,B,C) case A: return C;
        JOBSTATUS_MAP(JOBSTATUS_STATUSTEXT)
        default: break;
    }

    return QObject::tr("Undefined");
}

ProgramInfo *JobInfo::getProgramInfo(void)
{
    if (m_pgInfo == NULL)
    {
        if (!IsRecording())
            return m_pgInfo;

        m_pgInfo = new ProgramInfo(m_chanid, m_starttime);
        if (!m_pgInfo->GetChanID())
        {
            VERBOSE(VB_JOBQUEUE, LOC_ERR +
                QString("Unable to retrieve program info for "
                        "chanid %1 @ %2")
                    .arg(m_chanid).arg(m_starttime.toString()));

            setStatus(JOB_ERRORED);
            saveComment("Unable to retrieve Program Info from database");

            delete m_pgInfo;
            m_pgInfo = NULL;
            return NULL;
        }
    }
    return m_pgInfo;
}

bool JobInfo::IsRecording()
{
    return bool(m_chanid) && (!m_starttime.isNull());
}

JobCommand *JobInfo::getCommand(void)
{
    if (m_command == NULL)
    {
        if (m_cmdid == -1)
            return m_command;

        m_command = new JobCommand(m_cmdid);
        if (!m_command->isStored())
        {
            VERBOSE(VB_JOBQUEUE, LOC_ERR +
                QString("Unable to retrieve command information for job %1")
                    .arg(m_cmdid));
            setStatus(JOB_ERRORED);
            saveComment("Unable to retrieve job command information.");

            delete m_command;
            m_command = NULL;
            return NULL;
        }
    }
    return m_command;
}

void JobInfo::PrintToLog()
{
    VERBOSE(VB_JOBQUEUE, "Dumping JobInfo instance");
    VERBOSE(VB_JOBQUEUE, QString("   jobid:         %1").arg(m_jobid));
    VERBOSE(VB_JOBQUEUE, QString("   cmdid:         %1").arg(m_cmdid));
    VERBOSE(VB_JOBQUEUE, QString("   chanid:        %1").arg(m_chanid));
    VERBOSE(VB_JOBQUEUE, QString("   starttime:     %1").arg(m_starttime.toString(Qt::ISODate)));
    VERBOSE(VB_JOBQUEUE, QString("   status:        %1").arg(m_status));
    VERBOSE(VB_JOBQUEUE, QString("   comment:       %1").arg(m_comment));
    VERBOSE(VB_JOBQUEUE, QString("   statustime:    %1").arg(m_statustime.toString(Qt::ISODate)));
    VERBOSE(VB_JOBQUEUE, QString("   hostname:      %1").arg(m_hostname));
    VERBOSE(VB_JOBQUEUE, QString("   schedruntime:  %1").arg(m_schedruntime.toString(Qt::ISODate)));
    VERBOSE(VB_JOBQUEUE, QString("   cputime:       %1").arg(m_cputime));
    VERBOSE(VB_JOBQUEUE, QString("   duration:      %1").arg(m_duration));
}

