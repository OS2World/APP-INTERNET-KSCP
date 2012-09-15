#include "KSCPClient.h"

#include "KRemoteWorkThread.h"

void KRemoteWorkThread::ThreadMain( void* arg )
{
    KSCPClient::RemoteParam* rpParam =
                    reinterpret_cast< KSCPClient::RemoteParam*>( arg );

    rpParam->pkscpc->RemoteMain( rpParam->pCallback );
}

