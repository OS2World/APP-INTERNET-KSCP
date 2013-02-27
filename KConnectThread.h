/*
 * Copyright (c) 2012-2013 by KO Myung-Hun <komh@chollian.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials
 *   provided with the distribution.
 *
 *   Neither the name of the copyright holder nor the names
 *   of any other contributors may be used to endorse or
 *   promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

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

