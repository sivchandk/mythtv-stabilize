#include "backendcontext.h"

QMap<int, EncoderLink *> tvList;
AutoExpire    *expirer      = NULL;
Scheduler     *sched        = NULL;
JobScheduler  *jobsched     = NULL;
HouseKeeper   *housekeeping = NULL;
MediaServer   *g_pUPnp      = NULL;
QString        pidfile;
QString        logfile;
