
#include <QString>
#include <QStringList>
#include <QReadLocker>
#include <QWriteLocker>

#include "mythsocket.h"
#include "mythverbose.h"
#include "jobscheduler.h"
#include "jobinfodb.h"
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

    SyncWithDB();

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
    if (command == "GET_INFO")
        return HandleGetInfo(socket, commands);
    if (command == "GET_LIST")
        return HandleGetJobList(socket);
    if (command == "GET_HOSTS")
        return HandleGetHostList(socket);
    if (command == "RUN_SCHEDULER")
        return HandleRunScheduler(socket);

    // all following commands must follow the syntax
    // QUERY_JOBQUEUE <command>[]:[]<jobinfo>
    
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

    if (!tmpjob.isValid())
    {
        res << "ERROR" << "invalid_job";
        socket->writeStringList(res);
        return true;
    }

    // all following commands must match to an existing job
    JobInfoDB *job = GetJobByID(tmpjob.getJobID());
    if (job == NULL)
    {
        res << "ERROR" << "job_not_in_queue";
        socket->writeStringList(res);
        return true;
    }

    job->UpRef();

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
    job = NULL;

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

bool JobScheduler::HandleGetHostList(MythSocket *socket)
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

void JobScheduler::SyncWithDB(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT id FROM jobqueue;");
    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::QueueJob()", query);
        return;
    }

    QList<int> newids;

    JobList::const_iterator jit;
    while (query.next())
    {
        QReadLocker rlock(&m_jobLock);
        bool found = false;
        int jobid = query.value(0).toInt();

        for (jit = m_jobList.begin(); jit != m_jobList.end(); ++jit)
        {
            if ((*jit)->getJobID() == jobid)
            {
                found = true;
                break;
            }
        }

        if (!found)
            newids << jobid;
    }

    if (newids.isEmpty())
        return;

    JobInfoDB *job;
    QWriteLocker wlock(&m_jobLock);
    QList<int>::const_iterator idit;
    for (idit = newids.begin(); idit != newids.end(); ++idit)
    {
        job = new JobInfoDB(*idit);
        m_jobList << job;
    }
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
                                         int jobType)
{
    QReadLocker rlock(&m_jobLock);

    JobInfoDB *job;

    JobList::const_iterator i;
    for (i = m_jobList.begin(); i != m_jobList.end(); ++i)
    {
        if ((*i)->getChanID() == chanid &&
            (*i)->getStartTime() == starttime &&
            (*i)->getJobType() == jobType)
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
        m_parent->SyncWithDB();
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
