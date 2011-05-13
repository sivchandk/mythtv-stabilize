

#include <QString>
#include <QTime>
#include <QDateTime>

#include <math.h>

#include "jobqueue.h"
#include "jobinfodb.h"
#include "jobscheduler.h"
#include "mythversion.h"
#include "referencecounter.h"

extern JobScheduler *jobsched;

DTC::JobCommandList *JobQueue::GetJobCommandList( int StartIndex, int Count )
{
    DTC::JobCommandList *dtcCommandList = new DTC::JobCommandList();

    QList<JobCommandDB*> commandlist = jobsched->GetCommandList();

    StartIndex   = min( StartIndex, (int)commandlist.size() );
    Count        = (Count > 0) ? min( Count, (int)commandlist.size() ) : commandlist.size();
    int EndIndex = min( (StartIndex + Count), (int)commandlist.size() );

    for (int n = StartIndex; n < EndIndex; n++)
        GetJobCommand(commandlist.at(n), dtcCommandList);

    int curPage = 0, totalPages = 0;
    if (Count == 0)
        totalPages = 1;
    else
        totalPages = (int)ceil((float)commandlist.size() / Count);

    if (totalPages == 1)
        curPage = 1;
    else
        curPage = (int)ceil((float)StartIndex / Count) + 1;

    dtcCommandList->setStartIndex    ( StartIndex         );
    dtcCommandList->setCount         ( Count              );
    dtcCommandList->setCurrentPage   ( curPage            );
    dtcCommandList->setTotalPages    ( totalPages         );
    dtcCommandList->setTotalAvailable( commandlist.size() );
    dtcCommandList->setAsOf          ( QDateTime::currentDateTime() );
    dtcCommandList->setVersion       ( MYTH_BINARY_VERSION );
    dtcCommandList->setProtoVer      ( MYTH_PROTO_VERSION  );

    return dtcCommandList;
}

DTC::JobCommand *JobQueue::GetJobCommand( int CmdId )
{
    return GetJobCommand(jobsched->GetCommand(CmdId));
}

DTC::JobCommand *JobQueue::GetJobCommand( JobCommandDB *jobcmd, DTC::JobCommandList *parent )
{
    DTC::JobCommand *dtccmd = NULL;

    if (parent)
        dtccmd = parent->AddNewJobCommand();
    else
        dtccmd = new DTC::JobCommand();

    dtccmd->setCmdId( jobcmd->getCmdID() );
    dtccmd->setType( jobcmd->getType() );
    dtccmd->setName( jobcmd->getName() );
    dtccmd->setSubName( jobcmd->getSubName() );
    dtccmd->setShortDesc( jobcmd->getShortDesc() );
    dtccmd->setLongDesc( jobcmd->getLongDesc() );
    dtccmd->setPath( jobcmd->getPath() );
    dtccmd->setArgs( jobcmd->getArgs() );
    dtccmd->setDefault( jobcmd->doesNeedFile() );
    dtccmd->setNeedsFile( jobcmd->doesNeedFile() );
    dtccmd->setCPUIntense( jobcmd->isCPUIntense() );
    dtccmd->setDiskIntense( jobcmd->isDiskIntense() );
    dtccmd->setSequence( jobcmd->isSequence() );

    QList<JobHost*>::const_iterator it;
    QList<JobHost*> hostmap = jobcmd->GetEnabledHosts();

    for (it = hostmap.begin(); it != hostmap.end(); ++it)
    {
        DTC::JobHost *dtchost = dtccmd->AddNewJobHost();

        dtchost->setHostName( (*it)->getHostname() );
        dtchost->setRunBefore( (*it)->getRunBefore() );
        dtchost->setRunAfter( (*it)->getRunAfter() );
        dtchost->setTerminate( (*it)->getTerminate() );
        dtchost->setIdleMax( (*it)->getIdleMax() );
        dtchost->setCPUMax( (*it)->getCPUMax() );
    }

    jobcmd->DownRef();

    return dtccmd;
}

bool JobQueue::CreateJobCommand( const QString &Type, const QString &Name,
        const QString &SubName, const QString &ShortDesc, const QString &LongDesc,
        const QString &Path, const QString &Args, bool Default, bool NeedsFile,
        bool CPUIntense, bool DiskIntense, bool Sequence )
{
    JobCommandDB *jobcmd = new JobCommandDB(Type, Name, SubName, ShortDesc,
                                            LongDesc, Path, Args, Default, NeedsFile,
                                            CPUIntense, DiskIntense, Sequence);
    ReferenceLocker rlock(jobcmd, false);
    if (!jobcmd->isStored())
        return false;
    return true;
}

bool JobQueue::UpdateJobCommand( int CmdId, const QString &Type, const QString &Name,
        const QString &SubName, const QString &ShortDesc, const QString &LongDesc,
        const QString &Path, const QString &Args, bool Default, bool NeedsFile,
        bool CPUIntense, bool DiskIntense, bool Sequence )
{
    JobCommandDB *jobcmd = jobsched->GetCommand(CmdId);
    if (jobcmd == NULL)
        return false;

    ReferenceLocker rlock(jobcmd, false);

    jobcmd->setType(Type);
    jobcmd->setName(Name);
    jobcmd->setSubName(SubName);
    jobcmd->setShortDesc(ShortDesc);
    jobcmd->setLongDesc(LongDesc);
    jobcmd->setPath(Path);
    jobcmd->setArgs(Args);
    jobcmd->setDefault(Default);
    jobcmd->setNeedsFile(NeedsFile);
    jobcmd->setCPUIntense(CPUIntense);
    jobcmd->setDiskIntense(DiskIntense);
    jobcmd->setSequence(Sequence);

    if (!jobcmd->SaveObject())
        return false;
    return true;
}

bool JobQueue::DeleteJobCommand( int CmdId )
{
    JobCommandDB *jobcmd = jobsched->GetCommand(CmdId);
    if (jobcmd == NULL)
        return false;

    ReferenceLocker rlock(jobcmd, false);
    if (!jobcmd->Delete())
        return false;
    return true;
}

bool JobQueue::CreateJobHost( int CmdId, const QString &HostName,
        const QTime &RunBefore, const QTime &RunAfter, bool Terminate,
        uint IdleMax, uint CPUMax )
{
    JobCommandDB *jobcmd = jobsched->GetCommand(CmdId);
    if (jobcmd == NULL)
        return false;
    ReferenceLocker cmdlock(jobcmd, false);

    if (jobcmd->ContainsHost(HostName))
        return false;

    JobHostDB *jobhost = new JobHostDB(CmdId, HostName, RunBefore, RunAfter,
                                       Terminate, IdleMax, CPUMax);
    ReferenceLocker hostlock(jobhost, false);
    if (!jobhost->isStored())
        return false;
    return true;
}

bool JobQueue::UpdateJobHost( int CmdId, const QString &HostName,
        const QTime &RunBefore, const QTime &RunAfter, bool Terminate,
        uint IdleMax, uint CPUMax )
{
    JobCommandDB *jobcmd = jobsched->GetCommand(CmdId);
    if (jobcmd == NULL)
        return false;
    ReferenceLocker cmdlock(jobcmd, false);

    JobHostDB *jobhost = (JobHostDB*)jobcmd->GetEnabledHost(HostName);
    if (jobhost == NULL)
        return false;
    ReferenceLocker hostlock(jobhost, false);

    jobhost->setRunBefore(RunBefore);
    jobhost->setRunAfter(RunAfter);
    jobhost->setTerminate(Terminate);
    jobhost->setIdleMax(IdleMax);
    jobhost->setCPUMax(CPUMax);

    if (!jobhost->SaveObject())
        return false;
    return true;
}

bool JobQueue::DeleteJobHost( int CmdId, const QString &HostName )
{
    JobCommandDB *jobcmd = jobsched->GetCommand(CmdId);
    if (jobcmd == NULL)
        return false;
    ReferenceLocker cmdlock(jobcmd, false);

    JobHostDB *jobhost = (JobHostDB*)jobcmd->GetEnabledHost(HostName);
    if (jobhost == NULL)
        return false;
    ReferenceLocker hostlock(jobhost, false);

    if (!jobhost->Delete())
        return false;
    return true;
}
