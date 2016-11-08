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

#define INCL_WIN
#include <os2.h>

#include <sstream>

#include "kscp.h"

#include "ServerInfoVector.h"

using namespace std;

#define KSCP_PRF_KEY_SERVER_COUNT "SERVER_COUNT"
#define KSCP_PRF_KEY_SERVER_BASE  "SERVER"

ServerInfoVector::~ServerInfoVector()
{
    vector< PSERVERINFO >::iterator it;

    for( it = _vtServerInfo.begin(); it != _vtServerInfo.end(); ++it )
        delete (*it);
}

void ServerInfoVector::Load()
{
    PSERVERINFO  psi;
    PCH          pchBuffer, pch;
    ULONG        ulBufMax;
    LONG         lCount = 0;
    stringstream ssKey;
    int          i;

    ulBufMax = sizeof( lCount );
    PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SERVER_COUNT, &lCount, &ulBufMax );

    for( i = 0; i < lCount; i++ )
    {
        ssKey.str("");
        ssKey << KSCP_PRF_KEY_SERVER_BASE << i;

        ulBufMax = MAX_ADDRESS_LEN  + MAX_USERNAME_LEN +
                   MAX_PASSWORD_LEN + MAX_DIR_LEN +
                   sizeof( int );

        pchBuffer = new CHAR[ ulBufMax ];

        PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                             ssKey.str().c_str(), pchBuffer, &ulBufMax );

        psi = new SERVERINFO;

        pch = pchBuffer;
        psi->strAddress = string( pch ).substr( 0, MAX_ADDRESS_LEN );

        pch += MAX_ADDRESS_LEN;
        psi->strUserName = string( pch ).substr( 0, MAX_USERNAME_LEN );

        pch += MAX_USERNAME_LEN;
        psi->strPassword = string( pch ).substr( 0, MAX_PASSWORD_LEN );

        pch += MAX_PASSWORD_LEN;
        psi->strDir = string( pch ).substr( 0, MAX_DIR_LEN );

        pch += MAX_DIR_LEN;
        psi->iAuth = *reinterpret_cast< int * >( pch );

        Add( psi );

        delete[] pchBuffer;
    }
}

void ServerInfoVector::Save()
{
    LONG         lCount = _vtServerInfo.size();
    stringstream ssKey;
    PCH          pchData, pch;
    LONG         lDataSize;

    PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SERVER_COUNT, &lCount,
                         sizeof( lCount ));

    for( int i = 0; i < lCount; ++i )
    {
        const PSERVERINFO psi = QueryServer( i );

        lDataSize = MAX_ADDRESS_LEN  + MAX_USERNAME_LEN +
                    MAX_PASSWORD_LEN + MAX_DIR_LEN +
                    sizeof( int );

        pchData = new CHAR[ lDataSize ];

        pch = pchData;
        memcpy( pch, psi->strAddress.c_str(), MAX_ADDRESS_LEN );

        pch += MAX_ADDRESS_LEN;
        memcpy( pch, psi->strUserName.c_str(), MAX_USERNAME_LEN );

        pch += MAX_USERNAME_LEN;
        memcpy( pch, psi->strPassword.c_str(), MAX_PASSWORD_LEN );

        pch += MAX_PASSWORD_LEN;
        memcpy( pch, psi->strDir.c_str(), MAX_DIR_LEN );

        pch += MAX_DIR_LEN;
        memcpy( pch, &psi->iAuth, sizeof( int ));

        ssKey.str("");
        ssKey << KSCP_PRF_KEY_SERVER_BASE << i;

        PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                             ssKey.str().c_str(), pchData, lDataSize );

        delete[] pchData;
    }
}

