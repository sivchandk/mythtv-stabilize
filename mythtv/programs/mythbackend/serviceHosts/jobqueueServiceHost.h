#ifndef JOBQUEUESERVICEHOST_H_
#define JOBQUEUESERVICEHOST_H_

#include "servicehost.h"
#include "services/jobqueue.h"

class JobQueueServiceHost : public ServiceHost
{
  public:
    JobQueueServiceHost( const QString sSharePath )
        : ServiceHost( JobQueue::staticMetaObject,
                       "JobQueue",
                       "/JobQueue",
                       sSharePath)
    {
    }

    virtual ~JobQueueServiceHost()
    {

    }
};

#endif

