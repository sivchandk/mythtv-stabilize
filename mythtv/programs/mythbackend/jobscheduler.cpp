
#include <QString>
#include <QStringList>
#include <QReadLocker>
#include <QWriteLocker>

#include "mythsocket.h"
#include "mythverbose.h"
#include "jobinfodb.h"
#include "jobscheduler.h"
#include "programinfo.h"

JobSchedulerPrivate *jobSchedThread = NULL;

JobQueueSocket::JobQueueSocket(QString hostname, MythSocket *socket) :
    m_hostname(hostname), m_socket(socket), m_disconnected(false),
    m_refCount(0)
{
}

JobQueueSocket::~JobQueueSocket()
{
    // any jobs remaining in the local list at socket close should be marked
    // as status unknown. if the jobqueue reconnects, the jobs can be recovered

    QWriteLocker wlock(&m_jobLock);

    JobList::iterator i;
    for (i = m_jobList.begin(); i != m_jobList.end(); ++i)
    {
        (*i)->saveStatus(JOB_UNKNOWN);
        (*i)->DownRef();
    }

    m_socket->DownRef();
}

void JobQueueSocket::UpRef(void)
{
    QMutexLocker locker(&m_refLock);
    m_refCount++;
}

bool JobQueueSocket::DownRef(void)
{
    QMutexLocker locker(&m_refLock);
    m_refCount--;
    if (m_refCount <= 0)
    {
        if (m_socket != NULL)
            m_socket->DownRef();

        delete this;
        return true;
    }
    return false;
}

bool JobQueueSocket::AddJob(JobInfoDB *job)
{
    QWriteLocker wlock(&m_jobLock);
    if (m_jobList.contains(job))
        return false;

    job->UpRef();
    m_jobList.append(job);

    VERBOSE(VB_IMPORTANT|VB_JOBQUEUE, QString("Dispatching job to host"));

    return job->Run(m_socket);
}

void JobQueueSocket::DeleteJob(JobInfoDB *job)
{
    QWriteLocker wlock(&m_jobLock);
    if (!m_jobList.contains(job))
        return;

    m_jobList.removeOne(job);
    job->DownRef();
}

JobList *JobQueueSocket::getAssignedJobs(void)
{
    QReadLocker rlock(&m_jobLock);
    JobList *list = new JobList(m_jobList);
    return list;
}

int JobQueueSocket::getAssignedJobCount(void)
{
    QReadLocker rlock(&m_jobLock);
    return m_jobList.count();
}

JobScheduler::JobScheduler()
{
    if (jobSchedThread)
    {
        jobSchedThread->stop();
        usleep(250000);
        delete jobSchedThread;
    }

    RefreshFromDB();

    jobSchedThread = new JobSchedulerCF(this);
    jobSchedThread->start();
}

void JobScheduler::connectionClosed(MythSocket *socket)
{
    JobHostMap::iterator it;
    bool match = false;

    // iterate through jobqueue list and close if socket matches connected queue
    {
        QReadLocker rlock(&m_hostLock);
        for (it = m_hostMap.begin(); it != m_hostMap.end(); ++it)
        {
            if ((*it)->getSocket() == socket)
            {
                match = true;
                break;
            }
        }
    }

    if (!match)
        return;

    JobQueueSocket *host = *it;
    VERBOSE(VB_IMPORTANT|VB_JOBQUEUE, QString("Jobqueue on host '%1'"
                        " has disconnected.").arg(host->getHostname()));

    {
        QWriteLocker wlock(&m_hostLock);
        m_hostMap.erase(it);
    }

    // mark assigned jobs as indeterminate state

    host->DownRef();
}

JobHostList *JobScheduler::GetConnectedQueues(void)
{
    QReadLocker rlock(&m_hostLock);

    JobHostList *hosts = new JobHostList();
    JobHostMap::iterator it;
    for (it = m_hostMap.begin(); it != m_hostMap.end(); ++it)
    {
        (*it)->UpRef();
        hosts->append(*it);
    }

    return hosts;
}

JobQueueSocket *JobScheduler::GetQueueByHostname(QString hostname)
{
    QReadLocker rlock(&m_hostLock);
    JobQueueSocket *socket = NULL;

    if (m_hostMap.contains(hostname))
        socket = m_hostMap[hostname];

    return socket;
}

bool JobScheduler::HandleAnnounce(MythSocket *socket, QStringList &commands,
                        QStringList &slist)
{
    QStringList res;

    if (commands[1] != "JobQueue")
        return false;

    if (commands.size() != 3)
    {
        res << "ERROR" << "improper_jobqueue_announce";
        socket->writeStringList(res);
        return true;
    }

    QString hostname = commands[2];

    {
        QReadLocker rlock(&m_hostLock);
        if (m_hostMap.contains(hostname))
        {
            res << "ERROR" << "duplicate_jobqueue_on_host";
            socket->writeStringList(res);
            return true;
        }
    }

    {
        QWriteLocker wlock(&m_hostLock);
        JobQueueSocket *jobsock = new JobQueueSocket(hostname, socket);
        m_hostMap.insert(hostname, jobsock);
        jobsock->UpRef();
    }

    socket->setAnnounce(slist);
    res << "OK";
    socket->writeStringList(res);
    jobSchedThread->wake();
    
    return true;
}

bool JobScheduler::HandleQuery(MythSocket *socket, QStringList &commands,
                     QStringList &slist)
{
    if (commands[0] != "QUERY_JOBQUEUE")
        return false;

    bool handled = false;
    QStringList res;

    QString command = commands[1];

    // returns one or more JobInfo
    if (command == "GET_INFO")
        return HandleGetInfo(socket, commands);
    if (command == "GET_LIST")
        return HandleGetJobList(socket);
    if (command == "GET_QUEUES")
        return HandleGetConnectedQueues(socket);

    // returns one or more JobCommand
    if (command == "GET_COMMAND")
        return HandleGetCommand(socket, commands);
    if (command == "GET_COMMANDS")
        return HandleGetCommands(socket);

    // returns one or more JobHost
    if (command == "GET_HOST")
        return HandleGetHost(socket, commands);
    if (command == "GET_HOSTS")
        return HandleGetHosts(socket, commands);

    if (command == "RUN_SCHEDULER")
        return HandleRunScheduler(socket);

    if (command == "QUEUE_JOB"  ||
        command == "SEND_INFO"  ||
        command == "PAUSE_JOB"  ||
        command == "RESUME_JOB" ||
        command == "STOP_JOB"   ||
        command == "RESTART_JOB")
    {
        // these commands must all pass a single JobInfo as
        // a stringlist

        if (slist.size() == 1)
        {
            res << "ERROR" << "invalid_job";
            socket->writeStringList(res);
            return true;
        }

        QStringList::const_iterator i = slist.begin();
        JobInfoDB tmpjob(++i, slist.end());

        if (command == "QUEUE_JOB")
            return HandleQueueJob(socket, tmpjob);

        // the following jobs must match an existing 
        // queued job

        if (tmpjob.getJobID() == -1)
        {
            res << "ERROR" << "invalid_job";
            socket->writeStringList(res);
            return true;
        }

        JobInfoDB *job = GetJobByID(tmpjob.getJobID());
        if (job == NULL)
        {
            res << "ERROR" << "job_not_in_queue";
            socket->writeStringList(res);
            return true;
        }

        if (command == "SEND_INFO")
            handled = HandleSendInfo(socket, tmpjob, job);
        else if (command == "PAUSE_JOB")
            handled = HandlePauseJob(socket, job);
        else if (command == "RESUME_JOB")
            handled = HandleResumeJob(socket, job);
        else if (command == "STOP_JOB")
            handled = HandleStopJob(socket, job);
        else if (command == "RESTART_JOB")
            handled = HandleRestartJob(socket, job);

        job->DownRef();
    }
    else if (command == "CREATE_COMMAND" ||
             command == "SEND_COMMAND" ||
             command == "DELETE_COMMAND")
    {
        if (slist.size() == 1)
        {
            res << "ERROR" << "invalid_command";
            socket->writeStringList(res);
            return true;
        }

        QStringList::const_iterator i = slist.begin();
        JobCommandDB tmpcmd(++i, slist.end());

        if (command == "CREATE_COMMAND")
            return HandleCreateCommand(socket, tmpcmd);

        if (tmpcmd.getCmdID() == -1)
        {
            res << "ERROR" << "invalid_command";
            socket->writeStringList(res);
            return true;
        }

        JobCommandDB *cmd = GetCommand(tmpcmd.getCmdID());
        if (cmd == NULL)
        {
            res << "ERROR" << "command_not_registered";
            socket->writeStringList(res);
            return true;
        }

        if (command == "SEND_COMMAND")
            handled = HandleSendCommand(socket, tmpcmd, cmd);
        else if (command == "DELETE_COMMAND")
            handled = HandleDeleteCommand(socket, cmd);

        cmd->DownRef();
    }
    else if (command == "ADD_HOST" ||
             command == "SEND_HOST" ||
             command == "DELETE_HOST")
    {
        if (slist.size() == 1)
        {
            res << "ERROR" << "invalid_command_host";
            socket->writeStringList(res);
            return true;
        }

        QStringList::const_iterator i = slist.begin();
        JobHostDB tmphost(++i, slist.end());

        JobCommandDB *cmd = GetCommand(tmphost.getCmdID());
        if (cmd == NULL)
        {
            res << "ERROR" << "command_not_registered";
            socket->writeStringList(res);
            return true;
        }

        if (command == "ADD_HOST")
        {
            bool res = HandleCreateHost(socket, tmphost);
            cmd->DownRef();
            return res;
        }

        JobHostDB *host = (JobHostDB*)cmd->GetEnabledHost(tmphost.getHostname());
        if (host == NULL)
        {
            res << "ERROR" << "commandhost_not_registered";
            socket->writeStringList(res);
            cmd->DownRef();
            return true;
        }

        if (command == "SEND_HOST")
            handled = HandleSendHost(socket, tmphost, host);
        else if (command == "DELETE_HOST")
            handled = HandleDeleteHost(socket, host);

        cmd->DownRef();
    }

    return handled;
}

bool JobScheduler::HandleGetInfo(MythSocket *socket, QStringList &commands)
{
    JobInfoDB *job;
    QStringList res;

    if (commands.size() == 5)
    {
        int chanid = commands[2].toInt();
        QDateTime starttime; starttime.setTime_t(commands[3].toUInt());
        int jobType = commands[4].toInt();
        job = GetJobByProgram(chanid, starttime, jobType);
    }
    else if (commands.size() == 3)
    {
        int jobid = commands[2].toInt();
        job = GetJobByID(jobid);
    }
    else
    {
        res << "ERROR" << "incorrect_command_count";
        socket->writeStringList(res);
        return true;
    }

    if (job == NULL)
    {
        res << "-1";
        socket->writeStringList(res);
        return true;
    }

    job->ToStringList(res);
    socket->writeStringList(res);
    return true;
}

bool JobScheduler::HandleGetJobList(MythSocket *socket)
{
    QStringList sl;

    QReadLocker rlock(&m_jobLock);
    sl << QString::number(m_jobList.size());

    JobList::const_iterator it;
    for (it = m_jobList.begin(); it != m_jobList.end(); ++it)
        (*it)->ToStringList(sl);

    socket->writeStringList(sl);
    return true;
}

bool JobScheduler::HandleGetConnectedQueues(MythSocket *socket)
{
    QStringList sl;

    QReadLocker rlock(&m_hostLock);
    sl << QString::number(m_hostMap.size());

    JobHostMap::const_iterator it;
    for (it = m_hostMap.begin(); it != m_hostMap.end(); ++it)
        sl << (*it)->getHostname();

    socket->writeStringList(sl);
    return true;
}

bool JobScheduler::HandleRunScheduler(MythSocket *socket)
{
    QStringList sl;
    sl << "OK";
    socket->writeStringList(sl);

    jobSchedThread->wake();
    return true;
}

bool JobScheduler::HandleSendInfo(MythSocket *socket, JobInfoDB &tmpjob,
                                  JobInfoDB *job)
{
    job->clone(tmpjob);

    QStringList sl;
    if (job->SaveObject())
        sl << "OK";
    else
        sl << "ERROR";
    socket->writeStringList(sl);

    return true;
}

bool JobScheduler::HandleQueueJob(MythSocket *socket, JobInfoDB &tmpjob)
{
    JobInfoDB *job = new JobInfoDB(tmpjob);
    QStringList res;

    if (!job->Queue())
        res << "ERROR" << "job_not_added";
    else
    {
        job->ToStringList(res);
        QWriteLocker wlock(&m_jobLock);
        m_jobList.append(job);
    }

    socket->writeStringList(res);
    jobSchedThread->wake();
    return true;
}

bool JobScheduler::HandlePauseJob(MythSocket *socket, JobInfoDB *job)
{
    QStringList res;

    if (job->getStatus() == JOB_RUNNING ||
        job->getStatus() == JOB_STARTING)
    {
        MythSocket *jobsocket = job->GetHost();
        if (jobsocket == NULL)
            res << "ERROR" << "job_not_on_running_host";
        else
        {
            QStringList sl;
            sl << "COMMAND_JOBQUEUE PAUSE";
            job->ToStringList(sl);
            if (!jobsocket->SendReceiveStringList(sl, 1) ||
                sl.empty() || sl[0] == "ERROR")
                res << "ERROR" << "job_could_not_be_paused";
            else
            {
                res << "OK";
                job->saveStatus(JOB_PAUSED);
            }
        }
    }
    else
        res << "ERROR" << "job_not_in_pausable_state";

    socket->writeStringList(res);
    return true;
}

bool JobScheduler::HandleResumeJob(MythSocket *socket, JobInfoDB *job)
{
    QStringList res;

    if (job->getStatus() == JOB_PAUSED)
    {
        MythSocket *jobsocket = job->GetHost();
        if (jobsocket == NULL)
            res << "ERROR" << "job_not_on_running_host";
        else
        {
            QStringList sl;
            sl << "COMMAND_JOBQUEUE RESUME";
            job->ToStringList(sl);
            if (!jobsocket->SendReceiveStringList(sl, 1) ||
                sl.empty() || sl[0] == "ERROR")
                res << "ERROR" << "job_could_not_be_resumed";
            else
            {
                res << "OK";
                job->saveStatus(JOB_RUNNING);
            }
        }
    }
    else
        res << "ERROR" << "job_not_in_resumable_state";

    socket->writeStringList(res);
    return true;
}

bool JobScheduler::HandleStopJob(MythSocket *socket, JobInfoDB *job)
{
    QStringList res;

    if (job->getStatus() == JOB_RUNNING ||
        job->getStatus() == JOB_STARTING||
        job->getStatus() == JOB_PAUSED)
    {
        MythSocket *jobsocket = job->GetHost();
        if (jobsocket == NULL)
            res << "ERROR" << "job_not_on_running_host";
        else
        {
            QStringList sl;
            sl << "COMMAND_JOBQUEUE STOP";
            job->ToStringList(sl);
            if (!jobsocket->SendReceiveStringList(sl, 1) ||
                sl.empty() || sl[0] == "ERROR")
                res << "ERROR" << "job_could_not_be_stopped";
            else
            {
                res << "OK";
                job->saveStatus(JOB_STOPPING);
            }
        }
    }
    else
        res << "ERROR" << "job_not_in_stopable_state";

    socket->writeStringList(res);
    return true;
}

bool JobScheduler::HandleRestartJob(MythSocket *socket, JobInfoDB *job)
{
    QStringList res;

    if (job->getStatus() == JOB_RUNNING ||
        job->getStatus() == JOB_STARTING)
    {
        MythSocket *jobsocket = job->GetHost();
        if (jobsocket == NULL)
            res << "ERROR" << "job_not_on_running_host";
        else
        {
            QStringList sl;
            sl << "COMMAND_JOBQUEUE RESTART";
            job->ToStringList(sl);
            if (!jobsocket->SendReceiveStringList(sl, 1) ||
                sl.empty() || sl[0] == "ERROR")
                res << "ERROR" << "job_could_not_be_restarted";
            else
            {
                res << "OK";
                job->saveStatus(JOB_STOPPING);
            }
        }
    }
    else
        res << "ERROR" << "job_not_in_restartable_state";

    socket->writeStringList(res);
    return true;
}

bool JobScheduler::HandleGetCommand(MythSocket *socket, QStringList &commands)
{
    QStringList sl;

    if (commands.size() != 2)
    {
        sl << "ERROR" << "invalid_call";
        socket->writeStringList(sl);
        return true;
    }

    JobCommandDB *cmd = GetCommand(commands[1].toInt());
    if (cmd == NULL)
    {
        sl << "ERROR" << "invalid_command";
        socket->writeStringList(sl);
        return true;
    }

    cmd->ToStringList(sl);
    socket->writeStringList(sl);
    cmd->DownRef();
    return true;
}

bool JobScheduler::HandleGetCommands(MythSocket *socket)
{
    QStringList sl;

    {
        QReadLocker rlock(&m_cmdLock);
        JobCommandMap::const_iterator it;

        sl << QString::number(m_cmdMap.size());
        for (it = m_cmdMap.begin(); it != m_cmdMap.end(); ++it)
            (*it)->ToStringList(sl);
    }

    socket->writeStringList(sl);
    return true;
}

bool JobScheduler::HandleSendCommand(MythSocket *socket, JobCommandDB &tmpcmd,
                                     JobCommandDB *cmd)
{
    cmd->clone(tmpcmd);

    QStringList sl;
    if (cmd->SaveObject())
        sl << "OK";
    else
        sl << "ERROR";
    socket->writeStringList(sl);

    return true;
}

bool JobScheduler::HandleCreateCommand(MythSocket *socket, JobCommandDB &tmpcmd)
{
    JobCommandDB *cmd = new JobCommandDB(tmpcmd);
    QStringList res;

    if (!cmd->Create())
        res << "ERROR" << "command_not_added";
    else
        cmd->ToStringList(res);

    socket->writeStringList(res);
    jobSchedThread->wake();
    return true;
}

bool JobScheduler::HandleDeleteCommand(MythSocket *socket, JobCommandDB *cmd)
{
//TODO: Needs to delete all matching jobs and hosts
    QStringList res;

    if (cmd->Delete())
        res << "OK";
    else
        res << "ERROR" << "command_not_deleted";

    socket->writeStringList(res);
    return true;
}

bool JobScheduler::HandleGetHost(MythSocket *socket, QStringList &commands)
{
    QStringList sl;

    if (commands.size() != 3)
    {
        sl << "ERROR" << "invalid_call";
        socket->writeStringList(sl);
        return true;
    }

    JobCommandDB *cmd = GetCommand(commands[1].toInt());
    if (cmd == NULL)
    {
        sl << "ERROR" << "invalid_command";
        socket->writeStringList(sl);
        return true;
    }

    JobHostDB *host = (JobHostDB*)cmd->GetEnabledHost(commands[2]);
    if (host == NULL)
    {
        sl << "ERROR" << "invalid_host";
        socket->writeStringList(sl);
        cmd->DownRef();
        return true;
    }

    host->ToStringList(sl);

    host->DownRef();
    cmd->DownRef();

    socket->writeStringList(sl);
    return true;
}

bool JobScheduler::HandleGetHosts(MythSocket *socket, QStringList &commands)
{
    QStringList sl;

    if (commands.size() != 2)
    {
        sl << "ERROR" << "invalid_call";
        socket->writeStringList(sl);
        return true;
    }

    JobCommandDB *cmd = GetCommand(commands[1].toInt());
    if (cmd == NULL)
    {
        sl << "ERROR" << "invalid_command";
        socket->writeStringList(sl);
        return true;
    }

    QList<JobHost*> hosts = cmd->GetEnabledHosts();
    cmd->DownRef();
    QList<JobHost*>::iterator it;

    sl << QString::number(hosts.size());
    for (it = hosts.begin(); it != hosts.end(); ++it)
    {
        (*it)->ToStringList(sl);
        (*it)->DownRef();
    }

    socket->writeStringList(sl);
    return true;
}

bool JobScheduler::HandleSendHost(MythSocket *socket, JobHostDB &tmphost,
                                  JobHostDB *host)
{
    host->clone(tmphost);

    QStringList sl;
    if (host->SaveObject())
        sl << "OK";
    else
        sl << "ERROR";
    socket->writeStringList(sl);

    return true;
}

bool JobScheduler::HandleCreateHost(MythSocket *socket, JobHostDB &tmphost)
{
    QStringList sl;
    JobHostDB *host = new JobHostDB(tmphost);

    if (!host->Create())
        sl << "ERROR" << "host_not_added";
    else
        host->ToStringList(sl);

    socket->writeStringList(sl);
    jobSchedThread->wake();
    return true;
}

bool JobScheduler::HandleDeleteHost(MythSocket *socket, JobHostDB *host)
{
    QStringList res;

    if (host->Delete())
        res << "OK";
    else
        res << "ERROR" << "host_not_deleted";

    socket->writeStringList(res);
    return true;
}

void JobScheduler::RefreshFromDB(void)
{
    // no sense being cautious with the locks, this one is going to be extremely invasive
    QWriteLocker clock(&m_cmdLock);
    QWriteLocker jlock(&m_jobLock);

    {
        // clear out any old references
        // perhaps this needs some blocking if there are actively running jobs
        // not sure when this might need to be run besides init through
        JobCommandMap::iterator cit;
        for (cit = m_cmdMap.begin(); cit != m_cmdMap.end(); ++cit)
            (*cit)->DownRef();
        m_cmdMap.clear();

        JobList::iterator jit;
        for (jit = m_jobList.begin(); jit != m_jobList.end(); ++jit)
            (*jit)->DownRef();
        m_jobList.clear();
    }

    // populate list of job commands
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cmdid, type, name, subname, shortdesc, longdesc, "
                         "path, args, needsfile, def, cpuintense, "
                         "diskintense, sequence "
                  "FROM jobcommand;");
    if (!query.exec())
        return;

    JobCommandDB *cmd = new JobCommandDB(query);
    while (cmd->isStored())
    {
        m_cmdMap.insert(cmd->getCmdID(), cmd);
        cmd = new JobCommandDB(query);
    }
    // the last one will always be invalid
    // no need to downref, just delete it directly
    delete cmd;
    cmd = NULL;

    // populate runnable hosts for each job command
    query.prepare("SELECT cmdid, hostname, runbefore, runafter, terminate, "
                         "idlemax, cpumax "
                  "FROM jobhost;");
    if (!query.exec())
        return;

    JobHostDB *host = new JobHostDB(query);
    while (host->isStored())
    {
        if (m_cmdMap.contains(host->getCmdID()))
            // nothing else can access this map, so no lock needed
            m_cmdMap[host->getCmdID()]
                    ->m_hostMap.insert(host->getHostname(), host);
        else
        {
            VERBOSE(VB_JOBQUEUE|VB_IMPORTANT,
                "Job host definition found without matching command.");
            delete host;
        }
        host = new JobHostDB(query);
    }
    delete host;
    host = NULL;

    // populate autorun tasks for each job command
    query.prepare("SELECT cmdid, recordid, realtime "
                  "FROM jobrecord;");
    if (!query.exec())
        return;

    while (query.next())
    {
        int cmdid     = query.value(0).toInt();
        int recordid  = query.value(1).toInt();
        bool realtime = (query.value(2) != 0);

        if (m_cmdMap.contains(cmdid))
            m_cmdMap[cmdid]->m_recordMap.insert(recordid, realtime);
        else
            VERBOSE(VB_JOBQUEUE|VB_IMPORTANT,
                "Job autorun definition found without matching command.");
    }

    // populate existing jobs in the queue
    query.prepare("SELECT jobid, cmdid, chanid, starttime, status, comment, "
                         "statustime, hostname, schedruntime, cputime, duration "
                  "FROM jobqueue");
    if (!query.exec())
        return;

    JobInfoDB *job = new JobInfoDB(query);
    while (job->isStored())
    {
        if (m_cmdMap.contains(job->getCmdID()))
        {
            job->m_command = m_cmdMap[job->getCmdID()];
            m_jobList.append(job);
        }
        else
            VERBOSE(VB_JOBQUEUE|VB_IMPORTANT,
                "Queued Job found with no known command.");
    }
    delete job;
    job = NULL;
}

QList<JobCommandDB*> JobScheduler::GetCommandList(void)
{
    QReadLocker rlock(&m_cmdLock);

    QList<JobCommandDB*> cmdlist;

    JobCommandMap::iterator it;
    for (it = m_cmdMap.begin(); it != m_cmdMap.end(); ++it)
    {
        (*it)->UpRef();
        cmdlist.append(*it);
    }

    return cmdlist;
}

JobCommandDB *JobScheduler::GetCommand(int cmdid)
{
    QReadLocker rlock(&m_cmdLock);

    JobCommandDB *jobcmd = NULL;
    if (m_cmdMap.contains(cmdid))
    {
        jobcmd = m_cmdMap[cmdid];
        jobcmd->UpRef();
    }

    return jobcmd;
}

JobList *JobScheduler::GetQueuedJobs(void)
{
    JobList *jl = new JobList();

    JobList::const_iterator i;
    for (i = m_jobList.begin(); i != m_jobList.end(); ++i)
    {
        if ((*i)->getStatus() == JOB_QUEUED)
        {
            (*i)->UpRef();
            jl->append(*i);
        }
    }

    return jl;
}

JobList *JobScheduler::GetJobByProgram(uint chanid, QDateTime starttime)
{
    JobList *jl = new JobList();

    JobList::const_iterator i;
    for (i = m_jobList.begin(); i != m_jobList.end(); ++i)
    {
        if ((*i)->getChanID() == chanid &&
            (*i)->getStartTime() == starttime)
        {
            (*i)->UpRef();
            jl->append(*i);
        }
    }

    return jl;
}

JobList *JobScheduler::GetJobByProgram(ProgramInfo *pg)
{
    return GetJobByProgram(pg->GetChanID(), pg->GetRecordingStartTime());
}

JobInfoDB *JobScheduler::GetJobByID(int jobid)
{
    QReadLocker rlock(&m_jobLock);

    JobInfoDB *job;

    JobList::const_iterator i;
    for (i = m_jobList.begin(); i != m_jobList.end(); ++i)
    {
        if ((*i)->getJobID() == jobid)
        {
            job = *i;
            job->UpRef();
            return job;
        }
    }

    return NULL;
}

JobInfoDB *JobScheduler::GetJobByProgram(uint chanid, QDateTime starttime,
                                         int cmdid)
{
    QReadLocker rlock(&m_jobLock);

    JobInfoDB *job;

    JobList::const_iterator i;
    for (i = m_jobList.begin(); i != m_jobList.end(); ++i)
    {
        if ((*i)->getChanID() == chanid &&
            (*i)->getStartTime() == starttime &&
            (*i)->getCmdID() == cmdid)
        {
            job = *i;
            job->UpRef();
            return job;
        }
    }

    return NULL;
}

JobInfoDB *JobScheduler::GetJobByProgram(ProgramInfo *pg, int jobType)
{
    return GetJobByProgram(pg->GetChanID(), pg->GetRecordingStartTime(),
                           jobType);
}

void JobScheduler::AddCommand(JobCommandDB *cmd)
{
    {
        QReadLocker rlock(&m_cmdLock);
        if (m_cmdMap.contains(cmd->getCmdID()))
            return;
    }

    cmd->UpRef();
    QWriteLocker wlock(&m_cmdLock);
    m_cmdMap.insert(cmd->getCmdID(), cmd);
}

void JobScheduler::DeleteCommand(JobCommandDB *cmd)
{
    {
        QReadLocker rlock(&m_cmdLock);
        if (!m_cmdMap.contains(cmd->getCmdID()))
            return;
    }

    QWriteLocker wlock(&m_cmdLock);
    m_cmdMap.remove(cmd->getCmdID());
    cmd->DownRef();
}

void JobScheduler::AddJob(JobInfoDB *job)
{
    {
        QReadLocker rlock(&m_jobLock);
        if (m_jobList.contains(job))
            return;
    }

    job->UpRef();
    QWriteLocker wlock(&m_jobLock);
    m_jobList.append(job);
}

void JobScheduler::DeleteJob(JobInfoDB *job)
{
    {
        QReadLocker rlock(&m_jobLock);
        if (!m_jobList.contains(job))
            return;
    }

    QWriteLocker wlock(&m_jobLock);
    m_jobList.removeOne(job);
    job->DownRef();
}

JobSchedulerPrivate::JobSchedulerPrivate(JobScheduler *parent)
    : QThread(), m_termthread(false)
{
    m_parent = parent;

    m_timer = new QTimer();
    m_timer->setSingleShot(true);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(wake()));
    m_timer->start(60000);
}

JobSchedulerPrivate::~JobSchedulerPrivate()
{
    m_timer->stop();
    delete m_timer;
}

void JobSchedulerPrivate::run(void)
{
    while (!m_termthread)
    {
        QMutexLocker locker(&m_lock);

        VERBOSE(VB_JOBQUEUE|VB_EXTRA, "Running job scheduler.");
        DoJobScheduling();

        m_timer->start(60000);

        m_waitCond.wait(locker.mutex());
    }
}

void JobSchedulerPrivate::wake(void)
{
    m_waitCond.wakeAll();
}

void JobSchedulerPrivate::stop(void)
{
    m_termthread = true;
    wake();
}

void JobSchedulerPrivate::DoJobScheduling(void)
{
    VERBOSE(VB_IMPORTANT, "This should never be triggered.");
}

void JobSchedulerCF::DoJobScheduling(void)
{
    JobList *jobs = m_parent->GetQueuedJobs();
    if (jobs->isEmpty())
    {
        VERBOSE(VB_JOBQUEUE|VB_EXTRA, "No jobs in queue.");
        delete jobs;
        return;
    }

    JobHostList *hosts = m_parent->GetConnectedQueues();
    if (hosts->isEmpty())
    {
        VERBOSE(VB_IMPORTANT|VB_JOBQUEUE, "Jobs in queue cannot be run, "
                                            "no jobqueue available.");

        JobList::iterator it;
        for (it = jobs->begin(); it != jobs->end(); ++it)
            (*it)->DownRef();

        delete jobs;
        delete hosts;
        return;
    }

    int jobmax;
    JobList::iterator job;

    while (!jobs->isEmpty())
    {
        JobHostList::iterator host = hosts->begin();
        while (host != hosts->end())
        {
            jobmax = gCoreContext->GetNumSettingOnHost(
                  "JobQueueMaxSimultaneousJobs", (*host)->getHostname(), 1);
            if (jobmax <= (*host)->getAssignedJobCount())
            {
                // limit has been reached, remove from list and
                // continue with next host
                (*host)->DownRef();
                hosts->erase(host);
                continue;
            }

            for (job = jobs->begin(); job != jobs->end(); ++job)
            {
                //if (host can run job type)
                //{
                    // start job on host and remove from list
                    // skip to next host, well come back to this one if jobs
                    // are still left after going through all of them
                    (*host)->AddJob(*job);
                    jobs->erase(job);
                    (*job)->DownRef();
                    break;
                //}
            }
            host++;
        }
    }

    JobList::iterator jit;
    for (jit = jobs->begin(); jit != jobs->end(); ++jit)
        (*jit)->DownRef();
    delete jobs;

    JobHostList::iterator hit;
    for (hit = hosts->begin(); hit != hosts->end(); ++hit)
        (*hit)->DownRef();
    delete hosts;
}
