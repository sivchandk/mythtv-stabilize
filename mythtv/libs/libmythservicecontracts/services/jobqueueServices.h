#ifndef JOBQUEUESERVICES_H_
#define JOBQUEUESERVICES_H_

#include <QString>
#include <QTime>

#include "service.h"
#include "datacontracts/jobCommandList.h"
#include "datacontracts/jobCommandAndHost.h"

class SERVICE_PUBLIC JobQueueServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );
    Q_CLASSINFO( "CreateJobCommand_Method", "POST" )
    Q_CLASSINFO( "UpdateJobCommand_Method", "POST" )
    Q_CLASSINFO( "DeleteJobCommand_Method", "POST" )
    Q_CLASSINFO( "CreateJobHost_Method", "POST" )
    Q_CLASSINFO( "UpdateJobHost_Method", "POST" )
    Q_CLASSINFO( "DeleteJobHost_Method", "POST" )

  public:

    JobQueueServices( QObject *parent = 0 ) : Service( parent )
    {
        DTC::JobCommandList::InitializeCustomTypes();
        DTC::JobCommand::InitializeCustomTypes();
        DTC::JobHost::InitializeCustomTypes();
    }

  public slots:

    virtual DTC::JobCommandList* GetJobCommandList  ( int StartIndex,
                                                      int Count ) = 0;

    virtual DTC::JobCommand*     GetJobCommand      ( int CmdId ) = 0;

    virtual bool                 CreateJobCommand   ( const QString &Type,
                                                      const QString &Name,
                                                      const QString &SubName,
                                                      const QString &ShortDesc,
                                                      const QString &LongDesc,
                                                      const QString &Path,
                                                      const QString &Args,
                                                      bool          Default,
                                                      bool          NeedsFile,
                                                      bool          CPUIntense,
                                                      bool          DiskIntense,
                                                      bool          Sequence ) = 0;

    virtual bool                 UpdateJobCommand   ( int           CmdId,
                                                      const QString &Type,
                                                      const QString &Name,
                                                      const QString &SubName,
                                                      const QString &ShortDesc,
                                                      const QString &LongDesc,
                                                      const QString &Path,
                                                      const QString &Args,
                                                      bool          Default,
                                                      bool          NeedsFile,
                                                      bool          CPUIntense,
                                                      bool          DiskIntense,
                                                      bool          Sequence ) = 0;

    virtual bool                 DeleteJobCommand   ( int CmdId ) = 0;

    virtual bool                 CreateJobHost      ( int           CmdId,
                                                      const QString &HostName,
                                                      const QTime   &RunBefore,
                                                      const QTime   &RunAfter,
                                                      bool          Terminate,
                                                      uint          IdleMax,
                                                      uint          CPUMax ) = 0;

    virtual bool                 UpdateJobHost      ( int           CmdId,
                                                      const QString &HostName,
                                                      const QTime   &RunBefore,
                                                      const QTime   &RunAfter,
                                                      bool          Terminate,
                                                      uint          IdleMax,
                                                      uint          CPUMax ) = 0;

    virtual bool                 DeleteJobHost      ( int           CmdId,
                                                      const QString &HostName ) = 0;
};

#endif

