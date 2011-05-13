
using namespace std;

#include <QTimer>
#include <QString>
#include <QStringList>
#include <QReadLocker>
#include <QWriteLocker>


#include "mythsystem.h"
#include "mythsocket.h"
#include "jobsocket.h"
#include "jobinforun.h"
#include "jobinfo.h"

const uint kMasterServerReconnectTimeout = 1000;

JobSocketHandler::JobSocketHandler(void) :
    SocketRequestHandler(), m_serverSock(NULL),
    m_masterReconnect(NULL)
{
}

JobSocketHandler::~JobSocketHandler(void)
{
    // clean out m_jobList and terminate all running jobs
}

void JobSocketHandler::SetParent(MythSocketManager *parent)
{
    m_parent = parent;

    if (m_masterReconnect != NULL)
        delete m_masterReconnect;
    m_masterReconnect = new QTimer(this);

    m_masterReconnect->setSingleShot(true);
    connect(m_masterReconnect, SIGNAL(timeout()),
            this, SLOT(MasterReconnect()));
    m_masterReconnect->start(kMasterServerReconnectTimeout);
}

bool JobSocketHandler::HandleQuery(MythSocket *socket, QStringList &commands,
                                QStringList &slist)
{
    bool handled = false;

    if (commands[0] != "COMMAND_JOBQUEUE")
        return handled;

    // the following commands all must follow the syntax
    // COMMAND_JOBQUEUE <command>[]:[]<jobinfo>

    if ((commands.size() != 2) || (slist.size() < 2))
        return handled;
    QString command = commands[1];

    // pull job information
    QStringList::const_iterator i = slist.begin();
    JobInfoRun tmpjob(++i, slist.end());
    if (tmpjob.getJobID() == -1)
        return handled;

    if (command == "RUN")
    {
        HandleRunJob(socket, tmpjob);
        return true;
    }

    // match job information against local queue
    JobInfoRun *job;
    QStringList res;

    {
        QReadLocker rlock(&m_jobLock);
        if (!m_jobMap.contains(tmpjob.getJobID()))
        {
            res << "ERROR" << "job not found";
            socket->writeStringList(res);
            return true;
        }

        job = m_jobMap[tmpjob.getJobID()];
    }

    if (command == "PAUSE")
        handled = HandlePauseJob(socket, job);
    else if (command == "RESUME")
        handled = HandleResumeJob(socket, job);
    else if (command == "STOP")
        handled = HandleStopJob(socket, job);
    else if (command == "RESTART")
        handled = HandleRestartJob(socket, job);
    else if (command == "POLL")
        handled = HandlePollJob(socket, job);
    else
        return false;

    return true;
}

bool JobSocketHandler::HandleRunJob(MythSocket *socket, JobInfoRun &tmpjob)
{
    JobInfoRun *job = new JobInfoRun(tmpjob);
    QStringList res;

    QWriteLocker wlock(&m_jobLock);
    if (m_jobMap.contains(job->getJobID()))
    {
        res << "ERROR" << "job already exists";
        socket->writeStringList(res);
        return true;
    }

    m_jobMap.insert(job->getJobID(), job);
    if (job->Start())
        res << "OK";
    else
        res << "ERROR" << "job failed";
    socket->writeStringList(res);

    return true;
}

bool JobSocketHandler::HandlePauseJob(MythSocket *socket, JobInfoRun *job)
{
    QStringList res;
    if (job->getStatus() != JOB_RUNNING)
        res << "ERROR" << "job is not currently running";
    else if (job->Pause())
        res << "OK";
    else
        res << "ERROR" << "job could not be paused";

    socket->writeStringList(res);
    return true;
}

bool JobSocketHandler::HandleResumeJob(MythSocket *socket, JobInfoRun *job)
{
    QStringList res;
    if (job->getStatus() != JOB_PAUSED)
        res << "ERROR" << "job is not currently paused";
    else if (job->Resume())
        res << "OK";
    else
        res << "ERROR" << "job could not be resumed";

    socket->writeStringList(res);
    return true;
}

bool JobSocketHandler::HandleStopJob(MythSocket *socket, JobInfoRun *job)
{
    QStringList res;
    if (job->getStatus() != JOB_RUNNING)
        res << "ERROR" << "job is not currently running";
    else if (job->Stop())
        res << "OK";
    else
        res << "ERROR" << "job could not be stopped";

    socket->writeStringList(res);
    return true;
}

bool JobSocketHandler::HandleRestartJob(MythSocket *socket, JobInfoRun *job)
{
    QStringList res;
    if (job->getStatus() != JOB_RUNNING)
        res << "ERROR" << "job is not currently running";
    else if (job->Restart())
        res << "OK";
    else
        res << "ERROR" << "job could not be restarted";

    socket->writeStringList(res);
    return true;
}

bool JobSocketHandler::HandlePollJob(MythSocket *socket, JobInfoRun *job)
{
    QStringList res;
    job->ToStringList(res);
    socket->writeStringList(res);
    return true;
}

void JobSocketHandler::connectionClosed(MythSocket *sock)
{
    if (sock == m_serverSock)
        m_masterReconnect->start(kMasterServerReconnectTimeout);
    // TODO: come up with some auto-scaling timeout so its not hammering
    // the master backend once a second for connections
}

void JobSocketHandler::MasterReconnect(void)
{
    if (m_serverSock != NULL)
    {
        m_serverSock->DownRef();
        m_serverSock = NULL;
    }

    m_serverSock = new MythSocket();
    m_serverSock->UpRef();

    QString server = gCoreContext->GetSetting("MasterServerIP", "127.0.0.1");
    int port = gCoreContext->GetNumSetting("MasterServerPort", 6543);

    VERBOSE(VB_IMPORTANT, QString("Connecting to master server: %1:%2")
                           .arg(server).arg(port));

    if (!m_serverSock->connect(server, port))
    {
        VERBOSE(VB_IMPORTANT, "Connection to master server timed out.");
        m_masterReconnect->start(kMasterServerReconnectTimeout);
        m_serverSock->DownRef();
        m_serverSock = NULL;
        return;
    }

    if (m_serverSock->state() != MythSocket::Connected)
    {
        VERBOSE(VB_IMPORTANT, "Could not connect to master server.");
        m_masterReconnect->start(kMasterServerReconnectTimeout);
        m_serverSock->DownRef();
        m_serverSock = NULL;
        return;
    }

    if (!m_serverSock->Validate())
    {
        // verbose call handled by Validate()
        m_masterReconnect->start(kMasterServerReconnectTimeout);
        m_serverSock->DownRef();
        m_serverSock = NULL;
        return;
    }

    QStringList strlist;
    strlist << QString("ANN JobQueue %1").arg(gCoreContext->GetHostName()); 
    if (!m_serverSock->Announce(strlist))
    {
        // verbose call handled by Announce()
        m_masterReconnect->start(kMasterServerReconnectTimeout);
        m_serverSock->DownRef();
        m_serverSock = NULL;
        return;
    }

    if (strlist.empty() || strlist[0] == "ERROR")
    {
        VERBOSE(VB_IMPORTANT, "JobQueue announce to master server failed.");
        m_masterReconnect->start(kMasterServerReconnectTimeout);
        m_serverSock->DownRef();
        m_serverSock = NULL;
        return;
    }

    m_parent->AddConnection(m_serverSock);
    VERBOSE(VB_IMPORTANT, "JobQueue connected successfully.");

}
