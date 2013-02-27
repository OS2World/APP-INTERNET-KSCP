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

#ifndef KSCP_CONTAINER_H
#define KSCP_CONTAINER_H

#include <os2.h>

#include <KContainer.h>

#include <libssh2_sftp.h>

typedef struct _KSCPRECORD
{
    MINIRECORDCORE mrc;
    PSZ            pszName;
    PBYTE          pbAttr;
    PSZ            pszSize;
    PSZ            pszDate;
} KSCPRECORD, *PKSCPRECORD;

class KSCPContainer : public KContainer< KSCPRECORD, true >
{
protected :
    virtual SHORT KSortCompare( PKSCPRECORD p1, PKSCPRECORD p2,
                                PVOID pStorage )
    {
        LIBSSH2_SFTP_ATTRIBUTES* pattr1;
        LIBSSH2_SFTP_ATTRIBUTES* pattr2;

        int rc;

        pattr1 = reinterpret_cast< LIBSSH2_SFTP_ATTRIBUTES* >( p1->pbAttr );
        pattr2 = reinterpret_cast< LIBSSH2_SFTP_ATTRIBUTES* >( p2->pbAttr );

        // directory first
        rc = LIBSSH2_SFTP_S_ISDIR( pattr2->permissions ) -
             LIBSSH2_SFTP_S_ISDIR( pattr1->permissions );

        if( rc == 0 )
            rc = strcmp(  p1->pszName, p2->pszName );

        return rc;
    }
};
#endif
