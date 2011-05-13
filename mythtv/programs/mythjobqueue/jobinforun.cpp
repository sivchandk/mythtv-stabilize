using namespace std;

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QFileInfo>

#include "mythsystem.h"
#include "exitcodes.h"
#include "mythcorecontext.h"
#include "mythverbose.h"
#include "programinfo.h"
#include "jobinforun.h"
#include "recordingprofile.h"
#include "previewgenerator.h"
#include "mythdirs.h"

#define LOC QString("JobQueue: ")
#define LOC_ERR QString("JobQueue Error: ")

QString PrettyPrint(off_t bytes)
{
    // Pretty print "bytes" as KB, MB, GB, TB, etc., subject to the desired
    // number of units
    static const struct {
        const char      *suffix;
        unsigned int    max;
        int         precision;
    } pptab[] = {
        { "bytes", 9999, 0 },
        { "kB", 999, 0 },
        { "MB", 999, 1 },
        { "GB", 999, 1 },
        { "TB", 999, 1 },
        { "PB", 999, 1 },
        { "EB", 999, 1 },
        { "ZB", 999, 1 },
        { "YB", 0, 0 },
    };
    unsigned int    ii;
    float           fbytes = bytes;

    ii = 0;
    while (pptab[ii].max && fbytes > pptab[ii].max) {
        fbytes /= 1024;
        ii++;
    }

    return QString("%1 %2")
        .arg(fbytes, 0, 'f', pptab[ii].precision)
        .arg(pptab[ii].suffix);
}

bool JobInfoRun::Start(void)
{
    if (m_process != NULL)
    {
        VERBOSE(VB_IMPORTANT, "Attempting to run already running job.");
        return false;
    }

    QString command = GetJobCommand();
    if (command.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "JobQueue could not find command.");
        saveStatus(JOB_ERRORED, "Unable to find job command");
    }

    if (getStatus() == JOB_ERRORED)
        return false;

    m_process = new MythSystem(command, kMSRunShell);

    // setup nice and priority
    switch(gCoreContext->GetNumSetting("JobQueueCPU", 0))
    {
      case 0:
        m_process->nice(17);
        m_process->ioprio(8);
        break;
      case 1:
        m_process->nice(10);
        m_process->ioprio(7);
        break;
      case 2:
      default:
        break;
    }

    // startup announcements
    QString detailstr;
    if (IsRecording())
        detailstr = QString("%1 recorded from channel %2")
                    .arg(getProgramInfo()->toString(ProgramInfo::kTitleSubtitle))
                    .arg(getProgramInfo()->toString(ProgramInfo::kRecordingKey));
    else
        detailstr = QString("jobID %1").arg(m_jobid);

    QByteArray adetailstr = detailstr.toLocal8Bit();
    VERBOSE(VB_GENERAL, LOC + getCommand()->getName() + " Starting for "
                            + adetailstr.constData());
    gCoreContext->LogEntry("jobqueue", LP_NOTICE,
                           QString("%1 Starting").arg(getCommand()->getName()),
                           detailstr);

    // connect exit handlersa
    connect(m_process, SIGNAL(finished()), this, SLOT(finished()));
    connect(m_process, SIGNAL(error(uint)), this, SLOT(error(uint())));

    m_process->Run();

    // update status
    saveStatus(JOB_RUNNING);

    return true;
}

bool JobInfoRun::Stop(void)
{
    if (m_process == NULL)
        return false;

    m_process->Term();
    if (m_process->Wait(5) == GENERIC_EXIT_RUNNING)
        m_process->Kill();

    return true;
}

bool JobInfoRun::Pause(void)
{
    return true;
}

bool JobInfoRun::Resume(void)
{
    return true;
}

bool JobInfoRun::Restart(void)
{
    disconnect(m_process, SIGNAL(finished()), this, SLOT(finished()));
    disconnect(m_process, SIGNAL(error(uint)), this, SLOT(error(uint())));

    Stop();
    delete m_process;
    m_process = NULL;

    return Start();
}

void JobInfoRun::finished(void)
{
    QString msg;

    QueryObject();

    if (getProgramInfo())
        msg = QString("Finished %1 for %2 recorded from channel %3")
                .arg(getProgramInfo()->toString(ProgramInfo::kTitleSubtitle))
                .arg(getProgramInfo()->toString(ProgramInfo::kRecordingKey));
    else
        msg = QString("Finished %1 for jobID %2")
                    .arg(getCommand()->getName()).arg(m_jobid);

    QByteArray amsg = msg.toLocal8Bit();

    VERBOSE(VB_GENERAL, LOC + QString(amsg.constData()));

    gCoreContext->LogEntry("jobqueue", LP_NOTICE,
                    QString("Job \"%1\" Finished").arg(getCommand()->getName()),
                    msg);

    saveStatus(JOB_FINISHED, "Successfully Completed.");

    if (getProgramInfo())
        getProgramInfo()->SendUpdateEvent();

    delete m_process;
    m_process = NULL;
}

void JobInfoRun::error(uint error)
{
    QString msg, comment;

    QueryObject();

    if ((error == GENERIC_EXIT_DAEMONIZING_ERROR) ||
        (error == GENERIC_EXIT_CMD_NOT_FOUND))
    {
        msg = QString("Job '%1' failed, unable to find "
                      "executable. Check your PATH and backend logs.")
                        .arg(getCommand()->getName());
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
        VERBOSE(VB_IMPORTANT, LOC + QString("Current PATH: '%1'")
                                        .arg(getenv("PATH")));

        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                            "Job Errored", msg);

        saveStatus(JOB_ERRORED, 
            "ERROR: Unable to find executable, check backend logs.");
    }
    else if (error == GENERIC_EXIT_KILLED)
    {
        comment = tr("Aborted by user");
        saveStatus(JOB_ABORTED, comment);
        VERBOSE(VB_IMPORTANT, LOC_ERR + getCommand()->getName() + " " + comment);
        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                               getCommand()->getName(), comment);
    }
    else if (error == GENERIC_EXIT_NO_RECORDING_DATA)
    {
        comment = tr("Unable to open file or init decoder");
        saveStatus(JOB_ERRORED, comment);
        VERBOSE(VB_IMPORTANT, LOC_ERR + getCommand()->getName() + " " + comment);
        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                               getCommand()->getName(), comment);
    }
    else
    {
        msg = QString("Job '%1' failed, code: %2.")
                    .arg(GetJobCommand()).arg(error);
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);

        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                            "Job Errored", msg);

        saveStatus(JOB_ERRORED,
            "ERROR: Job returned non-zero, check logs.");
    }

    delete m_process;
    m_process = NULL;
}

QString JobInfoRun::GetJobCommand(void)
{
    if (!m_cmdstr.isEmpty())
        return m_cmdstr;

    ProgramInfo *pginfo = NULL;
    if (IsRecording())
    {
        pginfo = getProgramInfo();
        if (m_status == JOB_ERRORED)
            return m_cmdstr;
    }

    JobCommand *cmd = getCommand();
    QString cmdstr = QString("%1 %2").arg(cmd->getPath())
                                     .arg(cmd->getArgs());

    cmdstr.replace("%JOBID%", QString("%1").arg(m_jobid));

    if (IsRecording())
    {
        pginfo->SubstituteMatches(cmdstr);
        cmdstr.replace("%PREFIX%", GetInstallPrefix());
        cmdstr.replace("%VERBOSELEVEL%",
                    QString("%1").arg(print_verbose_messages));
    }

    m_cmdstr = cmdstr;
    return m_cmdstr;
}
