#ifndef KCONNECT_THREAD_H
#define KCONNECT_THREAD_H

#include <KPMSubThread.h>

#include "KSCPClient.h"

class KConnectThread : public KPMSubThread
{
protected :
    virtual void ThreadMain( void* arg )
    {
        void**      ppArg    =  reinterpret_cast< void** >( arg );
        KSCPClient* pkscpc   =  reinterpret_cast< KSCPClient* >( ppArg[ 0 ]);
        u_long      to_addr  = *reinterpret_cast< u_long* >( ppArg[ 1 ]);
        int         port     = *reinterpret_cast< int* >( ppArg[ 2 ]);
        int         timeout  = *reinterpret_cast< int* >( ppArg[ 3 ]);
        HEV         hevDone  = *reinterpret_cast< PHEV >( ppArg[ 4 ]);
        int*        piResult =  reinterpret_cast< int* >( ppArg[ 5 ]);

        pkscpc->ConnectMain( to_addr, port, timeout, hevDone, piResult );
    }
};
#endif

