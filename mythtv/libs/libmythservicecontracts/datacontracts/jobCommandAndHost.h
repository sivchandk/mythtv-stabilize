#ifndef JOBCOMMAND_H_
#define JOBCOMMAND_H_

#include <QString>
#include <QTime>

#include "serviceexp.h"
#include "datacontracthelper.h"


namespace DTC
{

 class SERVICE_PUBLIC JobHost : public QObject
 {
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0");

    Q_PROPERTY( QString     HostName        READ HostName       WRITE setHostName       )
    Q_PROPERTY( QTime       RunBefore       READ RunBefore      WRITE setRunBefore      )
    Q_PROPERTY( QTime       RunAfter        READ RunAfter       WRITE setRunAfter       )
    Q_PROPERTY( bool        Terminate       READ Terminate      WRITE setTerminate      )
    Q_PROPERTY( uint        IdleMax         READ IdleMax        WRITE setIdleMax        )
    Q_PROPERTY( uint        CPUMax          READ CPUMax         WRITE setCPUMax         )

    PROPERTYIMP( QString    , HostName  )
    PROPERTYIMP( QTime      , RunBefore )
    PROPERTYIMP( QTime      , RunAfter  )
    PROPERTYIMP( bool       , Terminate )
    PROPERTYIMP( uint       , IdleMax   )
    PROPERTYIMP( uint       , CPUMax    )

  public:

    static void InitializeCustomTypes()
    {
        qRegisterMetaType< JobHost  >();
        qRegisterMetaType< JobHost* >();
    }

    JobHost(QObject *parent = 0)
        : QObject       ( parent    ),
          m_HostName    ( ""        ),
          m_RunBefore   ( 23,59,59  ),
          m_RunAfter    ( 0, 0, 0   ),
          m_Terminate   ( false     ),
          m_IdleMax     ( 1800      ),
          m_CPUMax      ( 0         )
    {
    }

    JobHost( const JobHost &src )
    {
        Copy( src );
    }

    void Copy( const JobHost &src )
    {
        m_HostName      = src.m_HostName    ;
        m_RunBefore     = src.m_RunBefore   ;
        m_RunAfter      = src.m_RunAfter    ;
        m_Terminate     = src.m_Terminate   ;
        m_IdleMax       = src.m_IdleMax     ;
        m_CPUMax        = src.m_CPUMax      ;
    }

 };

 class SERVICE_PUBLIC JobCommand : public QObject
 {
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0");

    Q_CLASSINFO( "JobHosts_type", "DTC::JobHost");

    Q_PROPERTY( int          CmdId          READ CmdId          WRITE setCmdId          )
    Q_PROPERTY( QString      Type           READ Type           WRITE setType           )
    Q_PROPERTY( QString      Name           READ Name           WRITE setName           )
    Q_PROPERTY( QString      SubName        READ SubName        WRITE setSubName        )
    Q_PROPERTY( QString      ShortDesc      READ ShortDesc      WRITE setShortDesc      )
    Q_PROPERTY( QString      LongDesc       READ LongDesc       WRITE setLongDesc       )
    Q_PROPERTY( QString      Path           READ Path           WRITE setPath           )
    Q_PROPERTY( QString      Args           READ Args           WRITE setArgs           )
    Q_PROPERTY( bool         Default        READ Default        WRITE setDefault        )
    Q_PROPERTY( bool         NeedsFile      READ NeedsFile      WRITE setNeedsFile      )
    Q_PROPERTY( bool         CPUIntense     READ CPUIntense     WRITE setCPUIntense     )
    Q_PROPERTY( bool         DiskIntense    READ DiskIntense    WRITE setDiskIntense    )
    Q_PROPERTY( bool         Sequence       READ Sequence       WRITE setSequence       )

    Q_PROPERTY( QVariantList JobHosts       READ JobHosts       DESIGNABLE true         )

    PROPERTYIMP( int        , CmdId )
    PROPERTYIMP( QString    , Type )
    PROPERTYIMP( QString    , Name )
    PROPERTYIMP( QString    , SubName )
    PROPERTYIMP( QString    , ShortDesc )
    PROPERTYIMP( QString    , LongDesc )
    PROPERTYIMP( QString    , Path )
    PROPERTYIMP( QString    , Args )
    PROPERTYIMP( bool       , Default )
    PROPERTYIMP( bool       , NeedsFile )
    PROPERTYIMP( bool       , CPUIntense )
    PROPERTYIMP( bool       , DiskIntense )
    PROPERTYIMP( bool       , Sequence )

    PROPERTYIMP_RO_REF( QVariantList , JobHosts )

  public:

    static void InitializeCustomTypes()
    {
        qRegisterMetaType< JobCommand  >();
        qRegisterMetaType< JobCommand* >();

        JobHost::InitializeCustomTypes();
    }

    JobCommand(QObject *parent = 0)
        : QObject       ( parent    ),
          m_CmdId       ( -1        ),
          m_Type        ( ""        ),
          m_Name        ( ""        ),
          m_SubName     ( ""        ),
          m_ShortDesc   ( ""        ),
          m_LongDesc    ( ""        ),
          m_Path        ( ""        ),
          m_Args        ( ""        ),
          m_Default     ( false     ),
          m_NeedsFile   ( false     ),
          m_CPUIntense  ( false     ),
          m_DiskIntense ( false     ),
          m_Sequence    ( false     )
    {
    }

    JobCommand( const JobCommand &src )
    {
        Copy( src );
    }

    void Copy( const JobCommand &src )
    {
        m_CmdId         = src.m_CmdId;
        m_Type          = src.m_Type;
        m_Name          = src.m_Name;
        m_SubName       = src.m_SubName;
        m_ShortDesc     = src.m_ShortDesc;
        m_LongDesc      = src.m_LongDesc;
        m_Path          = src.m_Path;
        m_Args          = src.m_Args;
        m_Default       = src.m_Default;
        m_NeedsFile     = src.m_NeedsFile;
        m_CPUIntense    = src.m_CPUIntense;
        m_DiskIntense   = src.m_DiskIntense;
        m_Sequence      = src.m_Sequence;

        CopyListContents< JobHost >( this, m_JobHosts, src.m_JobHosts );
    }

    JobHost *AddNewJobHost()
    {
        JobHost *pObject = new JobHost( this );
        m_JobHosts.append( QVariant::fromValue<QObject *>( pObject ));

        return pObject;
    }

 };

}

Q_DECLARE_METATYPE( DTC::JobCommand )
Q_DECLARE_METATYPE( DTC::JobCommand* )

Q_DECLARE_METATYPE( DTC::JobHost )
Q_DECLARE_METATYPE( DTC::JobHost* )

#endif
