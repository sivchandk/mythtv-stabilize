
#include <QString>
#include <QStringList>
#include <QReadLocker>
#include <QWriteLocker>

#include "mythsocket.h"
#include "mythverbose.h"
#include "jobscheduler.h"
#include "jobinfodb.h"
#include "programinfo.h"

JobQueueSocket::JobQueueSocket(QString hostname, MythSocket *socket) :
    m_hostname(hostname), m_socket(socket), m_disconnected(false),
    m_refCount(1)
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

void JobScheduler::connectionClosed(MythSocket *socket)
{
    // iterate through jobqueue list and close if socket matches connected queue
}

JobHostList *JobScheduler::GetConnectedQueues(void)
{
    QReadLocker rlock(&m_hostLock);
    JobHostList *hosts = new JobHostList(m_hostMap.values());
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
    }

    socket->setAnnounce(slist);
    res << "OK";
    socket->writeStringList(res);
    
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
    if (!tmpjob.isValid())
    {
        res << "ERROR" << "invalid_job";
        socket->writeStringList(res);
        return true;
    }

    if (command == "QUEUE_JOB")
        return HandleQueueJob(socket, tmpjob);

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

bool JobScheduler::HandleSendInfo(MythSocket *socket, JobInfoDB &tmpjob,
                                  JobInfoDB *job)
{
    job->clone(tmpjob);
    job->SaveObject();
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
    : QThread()
{
    m_parent = parent;
    m_termthread = false;

    m_timer = new QTimer();
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

        VERBOSE(VB_GENERAL|VB_EXTRA, "Running job scheduler.");
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

void JobSchedulerCF::DoJobScheduling(void)
{
    JobList *jobs = m_parent->GetQueuedJobs();
    if (jobs->isEmpty())
    {
        delete jobs;
        return;
    }

    JobHostList *hosts = m_parent->GetConnectedQueues();
    if (hosts->isEmpty())
    {
        VERBOSE(VB_GENERAL, "Jobs in queue cannot be run, "
                            "no jobqueue available.");
        delete jobs;
        delete hosts;
        return;
    }

    int jobmax;
    JobList::iterator job;

    while (!jobs->isEmpty())
    {
        JobHostList::iterator host;
        for (host = hosts->begin(); host != hosts->end(); ++host)
        {
            jobmax = gCoreContext->GetNumSettingOnHost(
                  "JobQueueMaxSimultaneousJobs", (*host)->getHostname(), 1);
            if (jobmax > (*host)->getAssignedJobCount())
            {
                // limit has been reached, remove from list and
                // continue with next host
                hosts->removeOne(*host);
                continue;
            }

            for (job = jobs->begin(); job != jobs->end(); ++job)
            {
                //if (host can run job type)
                //{
                    // start job on host and remove from list
                    // skip to next host, well come back to this one if jobs
                    // are still left after going through all of them
                    jobs->removeOne(*job);
                    (*job)->UpRef();
                    (*host)->AddJob(*job);
                    break;
                //}
            }

            job = jobs->begin();
            while (jobmax > (*host)->getAssignedJobCount() &&
               job != jobs->end())
        {
            // host can accept new job
            // TODO: check if host can run type
            jobs->removeOne(*job);
            (*job)->UpRef();
            (*host)->AddJob(*job);
            job++;
        }

        if (jobs->isEmpty())
            // no more jobs to allocate
            break;
    }
    }

    delete jobs;
    delete hosts;
}
