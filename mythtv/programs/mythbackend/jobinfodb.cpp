
#include <QDateTime>

#include "mythsocket.h"
#include "mythdb.h"
#include "jobinfo.h"
#include "jobinfodb.h"

bool JobInfoDB::QueryObject(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    if (m_jobid)
    {
        query.prepare("SELECT id, chanid, starttime, inserttime, "
                             "type, cmds, flags, status, statustime, "
                             "hostname, args, comment, schedruntime "
                      "FROM jobqueue WHERE id = :JOBID;");
        query.bindValue(":JOBID", m_jobid);
    }
    else if (IsRecording())
    {
        query.prepare("SELECT id, chanid, starttime, inserttime, "
                             "type, cmds, flags, status, statustime, "
                             "hostname, args, comment, schedruntime "
                      "FROM jobqueue WHERE chanid    = :CHANID AND "
                                          "starttime = :STARTTIME AND "
                                          "type      = :JOBTYPE;");
        query.bindValue(":CHANID", m_chanid);
        query.bindValue(":STARTTIME", m_starttime);
        query.bindValue(":JOBTYPE", m_jobType);
    }
    else
        return false;

    if (!query.exec())
    {
        MythDB::DBError("Error in JobInfo::QueryObject()", query);
        return false;
    }

    if (query.next())
    {
        m_jobid         = query.value(0).toInt();
        m_chanid        = query.value(1).toInt();
        m_starttime     = query.value(2).toDateTime();
        m_inserttime    = query.value(3).toDateTime();
        m_jobType       = query.value(4).toUInt();
        m_cmds          = query.value(5).toInt();
        m_flags         = query.value(6).toInt();
        m_status        = query.value(7).toInt();
        m_statustime    = query.value(8).toDateTime();
        m_hostname      = query.value(9).toString();
        m_args          = query.value(10).toString();
        m_comment       = query.value(11).toString();
        m_schedruntime  = query.value(12).toDateTime();
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

    query.prepare("UPDATE jobqueue (chanid, starttime, inserttime, type, "
                                   "cmds, flags, status, statustime, "
                                   "hostname, args, comment, schedruntime) "
                  "VALUES (:CHANID, :STARTTIME, :TYPE, :CMDS, :FLAGS, "
                          ":STATUS, :HOSTNAME, :ARGS, :COMMENT, :SCHEDRUNTIME) "
                  "WHERE id = :JOBID;");

    query.bindValue(":JOBID",       m_jobid);
    query.bindValue(":CHANID",      m_chanid);
    query.bindValue(":STARTTIME",   m_starttime);
    query.bindValue(":TYPE",        m_jobType);
    query.bindValue(":CMDS",        m_cmds);
    query.bindValue(":FLAGS",       m_flags);
    query.bindValue(":STATUS",      m_status);
    query.bindValue(":HOSTNAME",    m_hostname);
    query.bindValue(":ARGS",        m_args);
    query.bindValue(":COMMENT",     m_comment);
    query.bindValue(":SCHEDRUNTIME",m_schedruntime);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobInfo::SaveObject()", query);
        return false;
    }

    return true;
}

bool JobInfoDB::Queue(void)
{
    if (m_jobid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    if (m_chanid)
    {
        query.prepare("SELECT status, id FROM jobqueue "
                      "WHERE chanid    = :CHANID AND "
                            "starttime = :STARTTIME AND "
                            "type      = :JOBTYPE;");
        query.bindValue(":CHANID", m_chanid);
        query.bindValue(":STARTTIME", m_starttime);
        query.bindValue(":JOBTYPE", m_jobType);

        if (!query.exec())
        {
            MythDB::DBError("Error in JobQueue::QueueJob()", query);
            return false;
        }

        if (query.next())
        {
            int status = query.value(0).toInt();

            switch (status)
            {
              case JOB_UNKNOWN:
              case JOB_STARTING:
              case JOB_RUNNING:
              case JOB_PAUSED:
              case JOB_STOPPING:
              case JOB_ERRORING:
              case JOB_ABORTING:
                return false;
              default:
                Delete(query.value(1).toInt());
            }
        }
    }

    query.prepare("INSERT INTO jobqueue (chanid, starttime, inserttime, type, "
                                        "status, statustime, schedruntime, "
                                        "hostname, args, comment, flags) "
            "VALUES (:CHANID, :STARTTIME, NOW(), :JOBTYPE, :STATUS, "
                    "NOW(), :SCHEDRUNTIME, :HOST, :ARGS, :COMMENT, :FLAGS);");
    query.bindValue(":CHANID", m_chanid);
    query.bindValue(":STARTTIME", m_starttime);
    query.bindValue(":JOBTYPE", m_jobType);
    query.bindValue(":STATUS", m_status);
    query.bindValue(":SCHEDRUNTIME", m_schedruntime);
    query.bindValue(":HOST", m_hostname);
    query.bindValue(":ARGS", m_args);
    query.bindValue(":COMMENT", m_comment);
    query.bindValue(":FLAGS", m_flags);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::QueueJob()", query);
        return false;
    }

    m_jobid = query.lastInsertId().toInt();
    m_inserttime = QDateTime::currentDateTime();
    m_statustime = QDateTime::currentDateTime();
    return true;
}

bool JobInfoDB::Delete(void)
{
    if (!m_jobid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT count(1) FROM jobqueue "
                  "WHERE id = :JOBID;");
    query.bindValue(":JOBID", m_jobid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::QueueJob()", query);
        return false;
    }

    if (query.next())
    {
        if (query.value(0).toInt() == 0)
            return false;
    }
    else
        return false;

    query.prepare("DELETE FROM jobqueue "
                  "WHERE id = :JOBID;");
    query.bindValue(":JOBID", m_jobid);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::QueueJob()", query);
        return false;
    }

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
    sl << "COMMAND_JOBQUEUE" << "RUN";
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
    sl << "COMMAND_JOBQUEUE" << "PAUSE";
    ToStringList(sl);

    if (!socket->SendReceiveStringList(sl) ||
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
    sl << "COMMAND_JOBQUEUE" << "RESUME";
    ToStringList(sl);

    if (!socket->SendReceiveStringList(sl) ||
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
    sl << "COMMAND_JOBQUEUE" << "STOP";
    ToStringList(sl);

    if (!socket->SendReceiveStringList(sl) ||
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
    sl << "COMMAND_JOBQUEUE" << "RESTART";
    ToStringList(sl);

    if (!socket->SendReceiveStringList(sl) ||
        sl[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "Scheduler failed to restart job");
        return false;
    }

    return true;
}

