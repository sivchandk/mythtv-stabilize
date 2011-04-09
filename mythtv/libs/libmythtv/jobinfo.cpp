
#include <unistd.h>
#include <iostream>
#include <cstdlib>
using namespace std;

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
#define DATETIME_TO_LIST(x)  INT_TO_LIST((x).toTime_t())
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
#define ENUM_FROM_LIST(x, y) do { NEXT_STR(); (x) = ((y)ts.toInt()); } while (0)
#define DATETIME_FROM_LIST(x) \
    do { NEXT_STR(); (x).setTime_t(ts.toUInt()); } while (0)
#define STR_FROM_LIST(x)     do { NEXT_STR(); (x) = ts; } while (0)


JobInfo::JobInfo(int id) :
     m_jobid(id), m_userJobIndex(-1)
{
    QueryObject();
}

JobInfo::JobInfo(uint chanid, QDateTime &starttime, int jobType) :
    m_userJobIndex(-1)
{
    QueryObject(chanid, starttime, jobType);
}

JobInfo::JobInfo(ProgramInfo &pginfo, int jobType) :
    m_userJobIndex(-1)
{
    QueryObject(pginfo.GetChanID(), pginfo.GetRecordingStartTime(), jobType);
}

JobInfo::JobInfo(int jobType, uint chanid, const QDateTime &starttime,
                 QString args, QString comment, QString host,
                 int flags, int status, QDateTime schedruntime) :
    m_chanid(chanid), m_starttime(starttime), m_jobType(jobType),
    m_flags(flags), m_status(status), m_hostname(host), m_args(args),
    m_comment(comment), m_schedruntime(schedruntime), m_userJobIndex(-1)
{
}

JobInfo::JobInfo(const JobInfo &other) :
    m_userJobIndex(-1)
{
    clone(other);
}

JobInfo::JobInfo(QStringList::const_iterator &it,
                 QStringList::const_iterator end) :
    m_userJobIndex(-1)
{
    FromStringList(it, end);
}

JobInfo::JobInfo(const QStringList &slist) :
    m_userJobIndex(-1)
{
    FromStringList(slist);
}

JobInfo::~JobInfo(void)
{

}

JobInfo &JobInfo::operator=(const JobInfo &other)
{
    clone(other);
    return *this;
}

void JobInfo::clone(const JobInfo &other)
{
    m_chanid = other.m_chanid;
    m_starttime = other.m_starttime;
    m_inserttime = other.m_inserttime;
    m_jobType = other.m_jobType;
    m_cmds = other.m_cmds;
    m_flags = other.m_flags;
    m_status = other.m_status;
    m_statustime = other.m_statustime;
    m_hostname = other.m_hostname;
    m_args = other.m_args;
    m_comment = other.m_comment;
    m_schedruntime = other.m_schedruntime;
}

void JobInfo::clear(void)
{
    m_jobid = 0;
    m_chanid = 0;
    m_starttime = QDateTime::currentDateTime();
    m_inserttime = m_starttime;
    m_jobType = 0;
    m_cmds = 0;
    m_flags = 0;
    m_status = 0;
    m_statustime = m_starttime;
    m_hostname.clear();
    m_args.clear();
    m_comment.clear();
    m_schedruntime = m_starttime;
}



bool JobInfo::SendExpectingInfo(QStringList &sl, bool clearinfo)
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

    return true;
}

bool JobInfo::ToStringList(QStringList &list) const
{
    INT_TO_LIST(m_jobid);
    INT_TO_LIST(m_chanid);
    DATETIME_TO_LIST(m_starttime);
    DATETIME_TO_LIST(m_inserttime);
    INT_TO_LIST(m_jobType);
    INT_TO_LIST(m_cmds);
    INT_TO_LIST(m_flags);
    INT_TO_LIST(m_status);
    DATETIME_TO_LIST(m_statustime);
    STR_TO_LIST(m_hostname);
    STR_TO_LIST(m_args);
    STR_TO_LIST(m_comment);
    DATETIME_TO_LIST(m_schedruntime);

    return true;
}

bool JobInfo::FromStringList(const QStringList &slist)
{
    QStringList::const_iterator it = slist.constBegin();
    return FromStringList(it, slist.constEnd());
}

bool JobInfo::FromStringList(QStringList::const_iterator &it,
                             QStringList::const_iterator listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;

    INT_FROM_LIST(m_jobid);
    INT_FROM_LIST(m_chanid);
    DATETIME_FROM_LIST(m_starttime);
    DATETIME_FROM_LIST(m_inserttime);
    INT_FROM_LIST(m_jobType);
    INT_FROM_LIST(m_cmds);
    INT_FROM_LIST(m_flags);
    INT_FROM_LIST(m_status);
    DATETIME_FROM_LIST(m_statustime);
    STR_FROM_LIST(m_hostname);
    STR_FROM_LIST(m_args);
    STR_FROM_LIST(m_comment);
    DATETIME_FROM_LIST(m_schedruntime);

    return true;
}

void JobInfo::setStatus(int status, QString comment)
{
    if (status != getStatus())
    {
        setStatus(status);
        setComment(comment);
    }
}

void JobInfo::saveStatus(int status, QString comment)
{
    if (status != getStatus())
    {
        setStatus(status);
        setComment(comment);
        SaveObject();
    }
}

bool JobInfo::QueryObject(void)
{
    QStringList sl(QString("QUERY_JOBQUEUE GET_INFO %s").arg(m_jobid));
    return SendExpectingInfo(sl, true);
}

bool JobInfo::QueryObject(int chanid, QDateTime starttime, int jobType)
{
    QStringList sl(QString("QUERY_JOBQUEUE GET_INFO %s %s %s")
                        .arg(chanid).arg(starttime.toString()).arg(jobType));
    return SendExpectingInfo(sl, true);
}

bool JobInfo::SaveObject(void)
{
    QStringList sl("QUERY_JOBQUEUE SEND_INFO");
    if (!ToStringList(sl))
        return false;
    return SendExpectingInfo(sl, false);
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


int JobInfo::GetUserJobIndex(void)
{
    if (m_userJobIndex < 0)
    {
        if (m_jobType & JOB_USERJOB)
        {
            int x = ((m_jobType & JOB_USERJOB) >> 8);
            int bits = 1;
            while ((x != 0) && ((x & 0x01) == 0))
            {
                bits++;
                x = x >> 1;
            }
            if (bits > 4)
                m_userJobIndex = 0;
        }
        else
            m_userJobIndex = 0;
    }

    return m_userJobIndex;
}

QString JobInfo::GetJobDescription(void)
{
    if (m_jobType == JOB_TRANSCODE)
        return "Transcode";
    else if (m_jobType == JOB_COMMFLAG)
        return "Commercial Detection";
    else if (!(m_jobType & JOB_USERJOB))
        return "Unknown Job";

    QString descSetting = 
        QString("UserJobDesc%1").arg(GetUserJobIndex());
    return gCoreContext->GetSetting(descSetting, "Unknown Job");
}

QString JobInfo::GetStatusText(void)
{
    switch (m_status)
    {
#define JOBSTATUS_STATUSTEXT(A,B,C) case A: return C;
        JOBSTATUS_MAP(JOBSTATUS_STATUSTEXT)
        default: break;
    }

    return tr("Undefined");
}

ProgramInfo *JobInfo::GetPGInfo(void)
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
            return NULL;
        }
    }
    return m_pgInfo;
}

bool JobInfo::IsRecording()
{
    return bool(m_chanid) && (!m_starttime.isNull());
}

void JobInfo::UpRef()
{
    QMutexLocker locker(&m_reflock);
    m_refcount++;
}

bool JobInfo::DownRef()
{
    m_reflock.lock();
    int count = --m_refcount;
    m_reflock.unlock();

    if (count <= 0)
    {
        delete this;
        return true;
    }

    return false;
}

