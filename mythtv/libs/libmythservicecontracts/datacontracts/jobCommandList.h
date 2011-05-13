#ifndef JOBCOMMANDLIST_H_
#define JOBCOMMANDLIST_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"
#include "jobCommandAndHost.h"

namespace DTC
{

 class SERVICE_PUBLIC JobCommandList : public QObject
 {
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_CLASSINFO( "JobCommands_type", "DTC::JobCommand");

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          CurrentPage    READ CurrentPage     WRITE setCurrentPage    )
    Q_PROPERTY( int          TotalPages     READ TotalPages      WRITE setTotalPages     )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList JobCommands READ JobCommands DESIGNABLE true )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , CurrentPage     )
    PROPERTYIMP       ( int         , TotalPages      )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, JobCommands )

  public:

    static void InitializeCustomTypes()
    {
        qRegisterMetaType< JobCommandList  >();
        qRegisterMetaType< JobCommandList* >();

        JobCommand::InitializeCustomTypes();
    }

    JobCommandList(QObject *parent = 0)
        : QObject( parent ),
          m_StartIndex    ( 0 ),
          m_Count         ( 0 ),
          m_TotalAvailable( 0 )
    {
    }

    JobCommandList( const JobCommandList &src )
    {
        Copy( src );
    }

    void Copy( const JobCommandList &src )
    {
            m_StartIndex    = src.m_StartIndex     ;
            m_Count         = src.m_Count          ;
            m_TotalAvailable= src.m_TotalAvailable ;
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;

            CopyListContents< JobCommand >( this, m_JobCommands, src.m_JobCommands );
    }

    JobCommand *AddNewJobCommand()
    {
        JobCommand *pObject = new JobCommand( this );
        m_JobCommands.append( QVariant::fromValue<QObject *>( pObject ));

        return pObject;
    }

 };

}

Q_DECLARE_METATYPE( DTC::JobCommandList  )
Q_DECLARE_METATYPE( DTC::JobCommandList* )

#endif
