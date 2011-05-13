
#include <QDateTime>

#include "mythsocket.h"
#include "mythdb.h"
#include "jobinfo.h"
#include "jobinfodb.h"
#include "jobscheduler.h"
#include "referencecounter.h"

extern JobScheduler *jobsched;

JobHostDB::JobHostDB(int cmdid, QString hostname) : JobHost()
{
    QueryObject(cmdid, hostname);
}

JobHostDB::JobHostDB(int cmdid, QString hostname, QTime runbefore,
              QTime runafter, bool terminate, uint idlemax, uint cpumax) :
    JobHost(cmdid, hostname, runbefore, runafter, terminate, idlemax,
            cpumax, false)
{
    Create();
}

bool JobHostDB::QueryObject(int cmdid, QString hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cmdid, hostname, runbefore, runafter, "
                         "terminate, idlemax, cpumax "
                  "FROM jobhost "
                  "WHERE cmdid = :CMDID AND hostname = :HOSTNAME;");
    query.bindValue(":CMDID", cmdid);
    query.bindValue(":HOSTNAME", hostname);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobHost::QueryObject()", query);
        return false;
    }

    return fromQuery(query);
}

bool JobHostDB::Create(void)
{
    JobCommandDB *jobcmd = jobsched->GetCommand(m_cmdid);
    if (jobcmd == NULL)
        return false;
    ReferenceLocker rlock(jobcmd, false);

    if (jobcmd->ContainsHost(m_hostname))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO jobhost (cmdid, hostname, runbefore, runafter,"
                                       "terminate, idlemax, cpumax) "
                  "VALUES (:CMDID, :HOSTNAME, :RUNBEFORE, :RUNAFTER, "
                          ":TERMINATE, :IDLEMAX, :CPUMAX);");
    query.bindValue(":CMDID", m_cmdid);
    query.bindValue(":HOSTNAME", m_hostname);
    query.bindValue(":RUNBEFORE", m_runbefore);
    query.bindValue(":RUNAFTER", m_runafter);
    query.bindValue(":TERMINATE", m_terminate);
    query.bindValue(":IDLEMAX", m_idlemax);
    query.bindValue(":CPUMAX", m_cpumax);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobHost::Create()", query);
        return false;
    }

    jobcmd->AddHost(this);
    setStored(true);

    return true;
}

bool JobHostDB::SaveObject(void)
{
    if (!isStored())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());;

    query.prepare("UPDATE jobhost SET "
                        "runbefore=:RUNBEFORE,"
                        "runafter=:RUNAFTER,"
                        "terminate=:TERMINATE,"
                        "idlemax=:IDLEMAX,"
                        "cpumax=:CPUMAX"
                  "WHERE cmdid = :CMDID AND hostname = :HOSTNAME;");
    query.bindValue(":CMDID", m_cmdid);
    query.bindValue(":HOSTNAME", m_hostname);
    query.bindValue(":RUNBEFORE", m_runbefore);
    query.bindValue(":RUNAFTER", m_runafter);
    query.bindValue(":TERMINATE", m_terminate);
    query.bindValue(":IDLEMAX", m_idlemax);
    query.bindValue(":CPUMAX", m_cpumax);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobHost::SaveObject()", query);
        return false;
    }

    return true;
}

bool JobHostDB::Delete(int cmdid, QString hostname)
{
    JobCommandDB *jobcmd = jobsched->GetCommand(cmdid);
    if (jobcmd == NULL)
        return false;
    ReferenceLocker cmdlock(jobcmd, false);

    JobHostDB *host = (JobHostDB*)jobcmd->GetEnabledHost(hostname);
    if (host == NULL)
        return false;
    ReferenceLocker hostlock(host, false);

    return host->Delete();
}

bool JobHostDB::Delete(void)
{
    if (!isStored())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM jobhost "
                  "WHERE cmdid = :CMDID AND hostname = :HOSTNAME;");
    query.bindValue(":CMDID", m_cmdid);
    query.bindValue(":HOSTNAME", m_hostname);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobHost::Delete()", query);
        return false;
    }

    JobCommandDB *jobcmd = jobsched->GetCommand(m_cmdid);
    if (jobcmd != NULL)
    {
        jobcmd->DeleteHost(this);
        jobcmd->DownRef();
    }

    if (query.numRowsAffected() == 0)
        // no rows deleted
        return false;

    return true;
}

bool JobHostDB::fromQuery(MSqlQuery &query)
{
    if (query.next())
    {
        m_cmdid         = query.value(0).toInt();
        m_hostname      = query.value(1).toString();
        m_runbefore     = query.value(2).toTime();
        m_runafter      = query.value(3).toTime();
        m_terminate     = (query.value(4).toInt() != 0);
        m_idlemax       = query.value(5).toUInt();
        m_cpumax        = query.value(6).toUInt();
    }
    else
        return false;

    setStored(true);
    return true;
}

JobCommandDB::JobCommandDB(int cmdid) : JobCommand()
{
    QueryObject(cmdid);
}

JobCommandDB::JobCommandDB(QString type, QString name, QString subname,
                 QString shortdesc, QString longdesc, QString path,
                 QString args, bool needsfile, bool rundefault,
                 bool cpuintense, bool diskintense, bool sequence) :
    JobCommand(type, name, subname, shortdesc, longdesc, path, args, needsfile,
               rundefault, cpuintense, diskintense, sequence, false)
{
    Create();
}

bool JobCommandDB::QueryObject(int cmdid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT type, name, subname, shortdesc, longdesc, "
                         "path, args, needsfile, def, cpuintensive, "
                         "diskintensive, sequence "
                   "FROM jobcommand "
                   "WHERE cmdid = :CMDID;");
    query.bindValue(":CMDID", cmdid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobCommand::QueryObject()", query);
        return false;
    }

    return fromQuery(query);
}

bool JobCommandDB::Create(void)
{
    if (isStored())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO jobcommand (type, name, subname, shortdesc, "
                                          "longdesc, path, args, needsfile, "
                                          "def, cpuintense, diskintense, "
                                          "sequence) "
                   "VALUES (:TYPE, :NAME, :SUBNAME, :SHORTDESC, :LONGDESC, "
                           ":PATH, :ARGS, :NEEDSFILE, :RUNDEFAULT, "
                           ":CPUINTENSIVE, :DISKINTENSIVE, :SEQUENCE);");
    query.bindValue(":TYPE", m_type);
    query.bindValue(":NAME", m_name);
    query.bindValue(":SUBNAME", m_subname);
    query.bindValue(":SHORTDESC", m_shortdesc);
    query.bindValue(":LONGDESC", m_longdesc);
    query.bindValue(":PATH", m_path);
    query.bindValue(":ARGS", m_args);
    query.bindValue(":NEEDSFILE", m_needsfile);
    query.bindValue(":RUNDEFAULT", m_rundefault);
    query.bindValue(":CPUINTENSIVE", m_cpuintense);
    query.bindValue(":DISKINTENSIVE", m_diskintense);
    query.bindValue(":SEQUENCE", m_sequence);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobCommand::Create()", query);
        return false;
    }

    jobsched->AddCommand(this);

    m_cmdid = query.lastInsertId().toInt();
    setStored(true);
    return true;
}

bool JobCommandDB::SaveObject(void)
{
    if (!isStored())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());;

    query.prepare("UPDATE jobcommand SET "
                    "type=:TYPE,"
                    "name=:NAME,"
                    "subname=:SUBNAME,"
                    "shortdesc=:SHORTDESC,"
                    "longdesc=:LONGDESC,"
                    "path=:PATH,"
                    "args=:ARGS,"
                    "needsfile=:NEEDSFILE,"
                    "def=:RUNDEFAULT,"
                    "cpuintense=:CPUINTENSIVE,"
                    "diskintense=:DISKINTENSIVE,"
                    "sequence=:SEQUENCE "
                  "WHERE cmdid = :CMDID;");
    query.bindValue(":CMDID", m_cmdid);
    query.bindValue(":TYPE", m_type);
    query.bindValue(":NAME", m_name);
    query.bindValue(":SUBNAME", m_subname);
    query.bindValue(":SHORTDESC", m_shortdesc);
    query.bindValue(":LONGDESC", m_longdesc);
    query.bindValue(":PATH", m_path);
    query.bindValue(":ARGS", m_args);
    query.bindValue(":NEEDSFILE", m_needsfile);
    query.bindValue(":RUNDEFAULT", m_rundefault);
    query.bindValue(":CPUINTENSIVE", m_cpuintense);
    query.bindValue(":DISKINTENSIVE", m_diskintense);
    query.bindValue(":SEQUENCE", m_sequence);
    
    if (!query.exec())
    {
        MythDB::DBError("Error in JobCommand::SaveObject()", query);
        return false;
    }

    return true;
}

bool JobCommandDB::Delete(void)
{
    if (!isStored())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM jobcommand "
                  "WHERE cmdid = :CMDID;");
    query.bindValue(":CMDID", m_cmdid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobCommand::Delete()", query);
        return false;
    }

    QList<JobHost*> hosts = GetEnabledHosts();
    QList<JobHost*>::iterator it;

    for (it = hosts.begin(); it != hosts.end(); ++it)
        (*it)->Delete();

    jobsched->DeleteCommand(this);

    if (query.numRowsAffected() == 0)
        // no rows deleted
        return false;

    return true;
}

bool JobCommandDB::Delete(int cmdid)
{
    JobCommandDB *jobcmd = jobsched->GetCommand(cmdid);
    if (jobcmd == NULL)
        return false;
    ReferenceLocker rlock(jobcmd, false);

    return jobcmd->Delete();
}

bool JobCommandDB::fromQuery(MSqlQuery &query)
{
    if (query.next())
    {
        m_cmdid         = query.value(0).toInt();
        m_type          = query.value(1).toString();
        m_name          = query.value(2).toString();
        m_subname       = query.value(3).toString();
        m_shortdesc     = query.value(4).toString();
        m_longdesc      = query.value(5).toString();
        m_path          = query.value(6).toString();
        m_args          = query.value(7).toString();
        m_needsfile     = (query.value(8).toInt() != 0);
        m_rundefault    = (query.value(9).toInt() != 0);
        m_cpuintense    = (query.value(10).toInt() != 0);
        m_diskintense   = (query.value(11).toInt() != 0);
        m_sequence      = (query.value(12).toInt() != 0);
    }
    else
        return false;

    setStored(true);
    return true;
}

QList<JobHost*> JobCommandDB::GetEnabledHosts(void)
{
    QList<JobHost*> jl;
    if (!isStored())
        return jl;

    QReadLocker rlock(&m_hostLock);

    QMap<QString, JobHost*>::iterator it;
    for (it = m_hostMap.begin(); it != m_hostMap.end(); ++it)
    {
        jl.append(*it);
        (*it)->UpRef();
    }

    return jl;
}

JobHost *JobCommandDB::GetEnabledHost(QString hostname)
{
    JobHost *host = NULL;
    QReadLocker rlock(&m_hostLock);

    if (m_hostMap.contains(hostname))
    {
        host = m_hostMap[hostname];
        host->UpRef();
    }

    return host;
}

bool JobCommandDB::ContainsHost(QString hostname)
{
    QReadLocker rlock(&m_hostLock);
    return m_hostMap.contains(hostname);
}

void JobCommandDB::AddHost(JobHostDB *host)
{
    {
        QReadLocker rlock(&m_hostLock);
        if (m_hostMap.contains(host->getHostname()))
            return;
    }

    host->UpRef();
    QWriteLocker wlock(&m_hostLock);
    m_hostMap.insert(host->getHostname(), host);
}

void JobCommandDB::DeleteHost(JobHostDB *host)
{
    {
        QReadLocker rlock(&m_hostLock);
        if (!m_hostMap.contains(host->getHostname()))
            return;
    }

    QWriteLocker wlock(&m_hostLock);
    m_hostMap.remove(host->getHostname());
    host->DownRef();
}

JobInfoDB::JobInfoDB(int id) : JobInfo()
{
    QueryObject(id);
}

JobInfoDB::JobInfoDB(uint chanid, QDateTime &starttime, int cmdid) :
        JobInfo()
{
    QueryObject(chanid, starttime, cmdid);
}

JobInfoDB::JobInfoDB(ProgramInfo &pginfo, int cmdid) :
    JobInfo()
{
    QueryObject(pginfo.GetChanID(), pginfo.GetRecordingStartTime(), cmdid);
}
    
JobInfoDB::JobInfoDB(int cmdid, uint chanid, const QDateTime &starttime,
                     int status, QString hostname, QDateTime schedruntime) :
    JobInfo(cmdid, chanid, starttime, status, hostname, schedruntime, false)
{
    Queue();
}

bool JobInfoDB::QueryObject(int jobid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("SELECT id, cmdid, chanid, starttime, status, "
                         "comment, statustime, hostname, schedruntime, "
                         "cputime, duration "
                  "FROM jobqueue WHERE id = :JOBID;");
    query.bindValue(":JOBID", jobid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobInfo::QueryObject()", query);
        return false;
    }

    return fromQuery(query);
}

bool JobInfoDB::QueryObject(uint chanid, QDateTime starttime, int cmdid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT id, cmdid, chanid, starttime, status, "
                         "comment, statustime, hostname, schedruntime, "
                         "cputime, duration "
                  "FROM jobqueue WHERE chanid    = :CHANID AND "
                                      "starttime = :STARTTIME AND "
                                      "cmdid     = :CMDID;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":CMDID", cmdid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobInfo::QueryObject()", query);
        return false;
    }

    return fromQuery(query);
}

bool JobInfoDB::fromQuery(MSqlQuery &query)
{
    if (query.next())
    {
        m_jobid         = query.value(0).toInt();
        m_cmdid         = query.value(1).toInt();
        m_chanid        = query.value(2).toInt();
        m_starttime     = query.value(3).toDateTime();
        m_status        = query.value(4).toInt();
        m_comment       = query.value(5).toString();
        m_statustime    = query.value(6).toDateTime();
        m_hostname      = query.value(7).toString();
        m_schedruntime  = query.value(8).toDateTime();
        m_cputime       = query.value(9).toInt();
        m_duration      = query.value(10).toInt();
    }
    else
        return false;

    return true;
}

bool JobInfoDB::SaveObject(void)
{
    if (!m_jobid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET "
                    "status=:STATUS, "
                    "comment=:COMMENT, "
                    "statustime=:STATUSTIME, "
                    "hostname=:HOSTNAME, "
                    "schedruntime=:SCHEDRUNTIME, "
                    "cputime=:CPUTIME, "
                    "duration=:DURATION "
                  "WHERE id = :JOBID;");

    query.bindValue(":STATUS",      m_status);
    query.bindValue(":COMMENT",     m_comment);
    query.bindValue(":STATUSTIME",  m_statustime);
    query.bindValue(":HOSTNAME",    m_hostname);
    query.bindValue(":SCHEDRUNTIME",m_schedruntime);
    query.bindValue(":CPUTIME",     m_cputime);
    query.bindValue(":DURATION",    m_duration);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobInfo::SaveObject()", query);
        return false;
    }

    return true;
}

bool JobInfoDB::Queue(void)
{
    if (isStored())
    {
        VERBOSE(VB_JOBQUEUE, "Refusing to queue a pre-existing job.");
        return false;
    }

    JobInfoDB *job = jobsched->GetJobByID(m_jobid);
    if (job->isStored())
    {
        VERBOSE(VB_JOBQUEUE, "Refusing to queue a pre-existing job.");
        job->DownRef();
        return false;
    }
    job->DownRef();

    if (IsRecording())
    {
        job = jobsched->GetJobByProgram(getProgramInfo(), m_cmdid);
        if (!(job->getStatus() & JOB_DONE))
        {
            VERBOSE(VB_JOBQUEUE, "Refusing to re-queue a running job.");
            job->DownRef();
            return false;
        }
        job->DownRef();
    }

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO jobqueue (cmdid, chanid, starttime, status, "
                                        "comment, statustime, hostname, "
                                        "schedruntime, cputime, duration) "
                 "VALUES (:CMDID, :CHANID, :STARTTIME, :STATUS, :COMMENT,"
                        " NOW(), :HOST, :SCHEDRUNTIME, :CPUTIME, :DURATION);");
    query.bindValue(":CMDID", m_cmdid);
    query.bindValue(":CHANID", m_chanid);
    query.bindValue(":STARTTIME", m_starttime);
    query.bindValue(":STATUS", m_status);
    query.bindValue(":COMMENT", m_comment);
    query.bindValue(":HOST", m_hostname);
    query.bindValue(":SCHEDRUNTIME", m_schedruntime);
    query.bindValue(":CPUTIME", m_cputime);
    query.bindValue(":DURATION", m_duration);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::QueueJob()", query);
        return false;
    }

    jobsched->AddJob(this);

    setStored(true);
    m_jobid = query.lastInsertId().toInt();
    m_statustime = QDateTime::currentDateTime();
    return true;
}

bool JobInfoDB::Delete(void)
{
    if (!m_jobid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM jobqueue "
                  "WHERE id = :JOBID;");
    query.bindValue(":JOBID", m_jobid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::Delete()", query);
        return false;
    }

    jobsched->DeleteJob(this);

    if (query.numRowsAffected() == 0)
        // no rows deleted
        return false;

    return true;
}

bool JobInfoDB::Delete(int jobID)
{
    JobInfoDB ji = JobInfoDB(jobID);
    return ji.Delete();
}

bool JobInfoDB::Run(MythSocket *socket)
{
    if (m_status != JOB_QUEUED)
    {
        VERBOSE(VB_IMPORTANT, "Scheduler tried to start job "
                              "not in queued state.");
        return false;
    }

    SetHost(socket);

    QStringList sl;
    sl << "COMMAND_JOBQUEUE RUN";
    ToStringList(sl);

    if (!socket->SendReceiveStringList(sl) ||
        sl[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "Scheduler failed to start job");
        return false;
    }

    return true;
}

bool JobInfoDB::Pause(void)
{
    if (m_hostSocket == NULL ||
        m_status != JOB_RUNNING)
    {
        return false;
    }

    QStringList sl;
    sl << "COMMAND_JOBQUEUE PAUSE";
    ToStringList(sl);

    if (!m_hostSocket->SendReceiveStringList(sl) ||
        sl[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "Scheduler failed to pause job");
        return false;
    }

    return true;
}

bool JobInfoDB::Resume(void)
{
    if (m_hostSocket == NULL ||
        m_status != JOB_PAUSED)
    {
        return false;
    }

    QStringList sl;
    sl << "COMMAND_JOBQUEUE RESUME";
    ToStringList(sl);

    if (!m_hostSocket->SendReceiveStringList(sl) ||
        sl[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "Scheduler failed to resume job");
        return false;
    }

    return true;
}

bool JobInfoDB::Stop(void)
{
    if (m_hostSocket == NULL ||
        m_status != JOB_RUNNING)
    {
        return false;
    }

    QStringList sl;
    sl << "COMMAND_JOBQUEUE STOP";
    ToStringList(sl);

    if (!m_hostSocket->SendReceiveStringList(sl) ||
        sl[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "Scheduler failed to stop job");
        return false;
    }

    return true;
}

bool JobInfoDB::Restart(void)
{
    if (m_hostSocket == NULL ||
        m_status != JOB_RUNNING)
    {
        return false;
    }

    QStringList sl;
    sl << "COMMAND_JOBQUEUE RESTART";
    ToStringList(sl);

    if (!m_hostSocket->SendReceiveStringList(sl) ||
        sl[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "Scheduler failed to restart job");
        return false;
    }

    return true;
}

