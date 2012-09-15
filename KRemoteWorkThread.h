#ifndef KREMOTE_WORK_THREAD_H
#define KREMOTE_WORK_THREAD_H

#include <KPMSubThread.h>

class KRemoteWorkThread : public KPMSubThread
{
public :
    KRemoteWorkThread() : KPMSubThread() {}
    virtual ~KRemoteWorkThread() {}

protected :
    virtual void ThreadMain( void* arg );
};
#endif
