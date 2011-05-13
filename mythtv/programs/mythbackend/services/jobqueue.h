#ifndef JOBQUEUE_H_
#define JOBQUEUE_H_

#include <QScriptEngine>

#include "services/jobqueueServices.h"
#include "jobinfodb.h"

class JobQueue : public JobQueueServices
{
    Q_OBJECT

  public:

    Q_INVOKABLE JobQueue( QObject *parent = 0 ) {};

  public slots:

    DTC::JobCommandList *GetJobCommandList  ( int StartIndex,
                                              int Count );

    DTC::JobCommand     *GetJobCommand      ( int CmdId );

    bool                 CreateJobCommand   ( const QString &Type,
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
                                              bool          Sequence );

    bool                 UpdateJobCommand   ( int           CmdId,
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
                                              bool          Sequence );

    bool                 DeleteJobCommand   ( int CmdId );

    bool                 CreateJobHost      ( int           CmdId,
                                              const QString &HostName,
                                              const QTime   &RunBefore,
                                              const QTime   &RunAfter,
                                              bool          Terminate,
                                              uint          IdleMax,
                                              uint          CPUMax );

    bool                 UpdateJobHost      ( int           CmdId,
                                              const QString &HostName,
                                              const QTime   &RunBefore,
                                              const QTime   &RunAfter,
                                              bool          Terminate,
                                              uint          IdleMax,
                                              uint          CPUMax );

    bool                 DeleteJobHost      ( int           CmdId,
                                              const QString &HostName );

  private:
    DTC::JobCommand     *GetJobCommand      ( JobCommandDB *jobcmd, DTC::JobCommandList *parent = 0 );
};

class ScriptableJobQueue : public QObject
{
    Q_OBJECT

  private:

    JobQueue m_obj;

  public:

    Q_INVOKABLE ScriptableJobQueue( QObject *parent = 0 ): QObject( parent ) {}

  public slots:

    DTC::JobCommandList *GetJobCommandList( int StartIndex, int Count )
    {
        return m_obj.GetJobCommandList(StartIndex, Count);
    }

    DTC::JobCommand     *GetJobCommand      ( int CmdId )
    {
        return m_obj.GetJobCommand(CmdId);
    }

    bool                 CreateJobCommand   ( const QString &Type,
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
                                              bool          Sequence )
    {
        return m_obj.CreateJobCommand(Type, Name, SubName, ShortDesc,
                                      LongDesc, Path, Args, Default, NeedsFile,
                                      CPUIntense, DiskIntense, Sequence);
    }

    bool                 UpdateJobCommand   ( int           CmdId,
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
                                              bool          Sequence )
    {
        return m_obj.UpdateJobCommand(CmdId, Type, Name, SubName, ShortDesc,
                                      LongDesc, Path, Args, Default, NeedsFile,
                                      CPUIntense, DiskIntense, Sequence);
    }

    bool                 DeleteJobCommand   ( int CmdId )
    {
        return m_obj.DeleteJobCommand(CmdId);
    }

    bool                 CreateJobHost      ( int           CmdId,
                                              const QString &HostName,
                                              const QTime   &RunBefore,
                                              const QTime   &RunAfter,
                                              bool          Terminate,
                                              uint          IdleMax,
                                              uint          CPUMax )
    {
        return m_obj.CreateJobHost(CmdId, HostName, RunBefore, RunAfter,
                                   Terminate, IdleMax, CPUMax);
    }

    bool                 UpdateJobHost      ( int           CmdId,
                                              const QString &HostName,
                                              const QTime   &RunBefore,
                                              const QTime   &RunAfter,
                                              bool          Terminate,
                                              uint          IdleMax,
                                              uint          CPUMax )
    {
        return m_obj.CreateJobHost(CmdId, HostName, RunBefore, RunAfter,
                                   Terminate, IdleMax, CPUMax);
    }

    bool                 DeleteJobHost      ( int           CmdId,
                                              const QString &HostName )
    {
        return m_obj.DeleteJobHost(CmdId, HostName);
    }
};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableJobQueue, QObject*);

#endif

