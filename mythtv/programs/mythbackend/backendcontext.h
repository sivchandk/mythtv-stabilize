#ifndef _BACKEND_CONTEXT_H_
#define _BACKEND_CONTEXT_H_

#include <QString>
#include <QMap>

class EncoderLink;
class AutoExpire;
class Scheduler;
class JobScheduler;
class HouseKeeper;
class MediaServer;

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire    *expirer;
extern Scheduler     *sched;
extern JobScheduler  *jobsched;
extern HouseKeeper   *housekeeping;
extern MediaServer   *g_pUPnp;
extern QString        pidfile;
extern QString        logfile;

class BackendContext
{
    
};

#endif // _BACKEND_CONTEXT_H_
