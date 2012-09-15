#ifndef KLOCAL_WORK_THREAD_H
#define KLOCAL_WORK_THREAD_H

#include <KPMSubThread.h>

class KLocalWorkThread : public KPMSubThread
{
public :
    KLocalWorkThread() : KPMSubThread() {}
    virtual ~KLocalWorkThread() {}

protected :
    virtual void ThreadMain( void* arg );
};
#endif
