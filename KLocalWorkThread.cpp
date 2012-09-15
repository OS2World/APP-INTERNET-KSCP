#include "KSCPClient.h"

#include "KLocalWorkThread.h"

void KLocalWorkThread::ThreadMain( void* arg )
{
    KSCPClient::LocalParam* lpParam =
                    reinterpret_cast< KSCPClient::LocalParam*>( arg );

    lpParam->pkscpc->LocalMain( lpParam->pCallback );
}

