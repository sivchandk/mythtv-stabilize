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
        m_status = JOB_ERRORED;
        m_comment = "Unable to find job command";
        SaveObject();
    }

    if (m_status == JOB_ERRORED)
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
                    .arg(GetPGInfo()->toString(ProgramInfo::kTitleSubtitle))
                    .arg(GetPGInfo()->toString(ProgramInfo::kRecordingKey));
    else
        detailstr = QString("jobID %1").arg(m_jobid);

    QByteArray adetailstr = detailstr.toLocal8Bit();
    VERBOSE(VB_GENERAL, LOC + GetJobDescription() + " Starting for "
                            + adetailstr.constData());
    gCoreContext->LogEntry("jobqueue", LP_NOTICE,
                           QString("%1 Starting").arg(GetJobDescription()),
                           detailstr);

    // connect exit handlers
    ConnectMS();

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
    DisconnectMS();

    Stop();
    delete m_process;
    m_process = NULL;

    return Start();
}

void JobInfoRun::ConnectMS(void)
{
    if (m_process == NULL)
        return;

    if (m_jobType == JOB_TRANSCODE)
    {
        connect(m_process, SIGNAL(finished()), this, SLOT(finishedts()));
        connect(m_process, SIGNAL(error(uint)), this, SLOT(errorts(uint())));
    }
    else if (m_jobType == JOB_COMMFLAG)
    {
        connect(m_process, SIGNAL(finished()), this, SLOT(finished()));
        connect(m_process, SIGNAL(error(uint)), this, SLOT(error(uint())));
    }
    else
    {
        connect(m_process, SIGNAL(finished()), this, SLOT(finished()));
        connect(m_process, SIGNAL(error(uint)), this, SLOT(error(uint())));
    }
}

void JobInfoRun::DisconnectMS(void)
{
    if (m_process == NULL)
        return;

    if (m_jobType == JOB_TRANSCODE)
    {
        disconnect(m_process, SIGNAL(finished()), this, SLOT(finishedts()));
        disconnect(m_process, SIGNAL(error(uint)), this, SLOT(errorts(uint())));
    }
    else if (m_jobType == JOB_COMMFLAG)
    {
        disconnect(m_process, SIGNAL(finished()), this, SLOT(finished()));
        disconnect(m_process, SIGNAL(error(uint)), this, SLOT(error(uint())));
    }
    else
    {
        disconnect(m_process, SIGNAL(finished()), this, SLOT(finished()));
        disconnect(m_process, SIGNAL(error(uint)), this, SLOT(error(uint())));
    }
}

void JobInfoRun::finished(void)
{
    QString msg;

    QueryObject();

    if (GetPGInfo())
        msg = QString("Finished %1 for %2 recorded from channel %3")
                .arg(GetPGInfo()->toString(ProgramInfo::kTitleSubtitle))
                .arg(GetPGInfo()->toString(ProgramInfo::kRecordingKey));
    else
        msg = QString("Finished %1 for jobID %2")
                    .arg(GetJobDescription()).arg(m_jobid);

    QByteArray amsg = msg.toLocal8Bit();

    VERBOSE(VB_GENERAL, LOC + QString(amsg.constData()));

    gCoreContext->LogEntry("jobqueue", LP_NOTICE,
                    QString("Job \"%1\" Finished").arg(GetJobDescription()),
                    msg);

    saveStatus(JOB_FINISHED, "Successfully Completed.");

    if (GetPGInfo())
        GetPGInfo()->SendUpdateEvent();

    delete m_process;
    m_process = NULL;
}

void JobInfoRun::finishedts(void)
{
    ProgramInfo *pginfo = GetPGInfo();
    QByteArray details;
    
    uint transcoder = pginfo->QueryTranscoderID();
    QString transcoderName = "Autodetect";
    if (transcoder != RecordingProfile::TranscoderAutodetect)
    {
        // some mysql stuff
        transcoderName = "NotAutodetect";
    }
    
    QString filename = pginfo->GetPlaybackURL(false, true);
    QFileInfo st(filename);

    if (st.exists())
    {
        long long filesize = st.size();
        long long origfilesize = 0;

        QString comment = QString("%1: %2 => %3")
                            .arg(transcoderName)
                            .arg(PrettyPrint(origfilesize))
                            .arg(PrettyPrint(filesize));
        saveComment(comment);

        if (filesize > 0)
            pginfo->SaveFilesize(filesize);

        details = (QString("%1: %2 (%3)")
                    .arg(pginfo->toString(ProgramInfo::kTitleSubtitle))
                    .arg(transcoderName)
                    .arg(PrettyPrint(filesize))).toLocal8Bit();
    }
    else
    {
        QString comment = QString("could not stat '%1'").arg(filename);
        saveComment(comment);

        details = (QString("%1: %2")
                    .arg(pginfo->toString(ProgramInfo::kTitleSubtitle))
                    .arg(comment)).toLocal8Bit();
    }

    pginfo->SaveTranscodeStatus(TRANSCODING_COMPLETE);

    QString msg = QString("Transcode %1").arg(GetStatusText());
    gCoreContext->LogEntry("transcode", LP_NOTICE, msg, details);
    VERBOSE(VB_GENERAL, LOC + msg + ": " + details);

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
                        .arg(GetJobDescription());
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
        VERBOSE(VB_IMPORTANT, LOC + QString("Current PATH: '%1'")
                                        .arg(getenv("PATH")));

        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                            "Job Errored", msg);

        saveStatus(JOB_ERRORED, 
            "ERROR: Unable to find executable, check backend logs.");
    }
    else if ((m_flags == JOB_STOP) ||
             (error == GENERIC_EXIT_KILLED))
    {
        comment = tr("Aborted by user");
        saveStatus(JOB_ABORTED, comment);
        VERBOSE(VB_IMPORTANT, LOC_ERR + GetJobDescription() + " " + comment);
        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                               GetJobDescription(), comment);
    }
    else if (error == GENERIC_EXIT_NO_RECORDING_DATA)
    {
        comment = tr("Unable to open file or init decoder");
        saveStatus(JOB_ERRORED, comment);
        VERBOSE(VB_IMPORTANT, LOC_ERR + GetJobDescription() + " " + comment);
        gCoreContext->LogEntry("jobqueue", LP_WARNING,
                               GetJobDescription(), comment);
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

void JobInfoRun::errorts(uint error)
{
    QString comment;

    if ((error == GENERIC_EXIT_DAEMONIZING_ERROR) ||
        (error == GENERIC_EXIT_CMD_NOT_FOUND))
    {
        comment = tr("Unable to find mythtranscode");
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Transcode failed: " + comment);
        saveStatus(JOB_ERRORED, comment);
    }
    else if ((m_flags == JOB_STOP) ||
             (error == GENERIC_EXIT_KILLED))
    {
        comment = tr("Aborted by user");
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Transcode " + comment);
        saveStatus(JOB_ABORTED, comment);
    }

    GetPGInfo()->SaveTranscodeStatus(TRANSCODING_NOT_TRANSCODED);

    delete m_process;
    m_process = NULL;
}

QString JobInfoRun::GetJobCommand(void)
{
    if (!m_command.isEmpty())
        return m_command;

    ProgramInfo *pginfo = NULL;
    uint transcoder = 0;
    if (IsRecording())
    {
        pginfo = GetPGInfo();
        if (m_status == JOB_ERRORED)
            return m_command;

        transcoder = pginfo->QueryTranscoderID();
    }


    if (m_jobType == JOB_TRANSCODE)
    {
        if (!IsRecording())
        {
            VERBOSE(VB_JOBQUEUE, LOC_ERR +
                "The JobQueue cannot currently transcode files that do not "
                "have a chanid/starttime in the recorded table.");
            setStatus(JOB_ERRORED);
            saveComment("ProgramInfo data not found");
            return m_command;
        }

        m_command = gCoreContext->GetSetting("JobQueueTranscodeCommand");

        if (m_command.trimmed().isEmpty() ||
                m_command == "mythtranscode")
        {
            // setup internal transcoder
            bool useCutlist = pginfo->HasCutlist() && 
                    !!(m_flags & JOB_USE_CUTLIST);

            m_command = QString("%PREFIX%/bin/mythtranscode "
                              "-j %JOBID% " 
                              "-V %VERBOSELEVEL% "
                              "-p %TRANSPROFILE% %1")
                .arg(useCutlist ? "-l" : "");
        }
    }
    else if (m_jobType == JOB_COMMFLAG)
    {
        if (!IsRecording())
        {
            VERBOSE(VB_JOBQUEUE, LOC_ERR +
                "The JobQueue cannot currently commflag files that do not "
                "have a chanid/starttime in the recorded table.");
            setStatus(JOB_ERRORED);
            saveComment("ProgramInfo data not found");
            return m_command;
        }

        m_command = gCoreContext->GetSetting("JobQueueCommFlagCommand");

        if (m_command.trimmed().isEmpty() ||
            m_command == "mythcommflag")
        {
            // setup internal commflagger
            m_command = QString("%PREFIX%/bin/mythcommflag "
                              "-j %JOBID% "
                              "-V %VERBOSELEVEL");
        }
    }
    else if (m_jobType & JOB_USERJOB)
    {
        m_command = gCoreContext->GetSetting(
                    QString("UserJob%1").arg(GetUserJobIndex()), "");
    }

    if (!m_command.isEmpty())
        m_command.replace("%JOBID%", QString("%1").arg(m_jobid));

    if (!m_command.isEmpty() && IsRecording())
    {
        pginfo->SubstituteMatches(m_command);
        m_command.replace("%PREFIX%", GetInstallPrefix());
        m_command.replace("%VERBOSELEVEL%",
                    QString("%1").arg(print_verbose_messages));
        m_command.replace("%TRANSPROFILE%",
                    (RecordingProfile::TranscoderAutodetect == transcoder) ?
                     "autodetect" : QString::number(transcoder));
    }

    return m_command;
}
