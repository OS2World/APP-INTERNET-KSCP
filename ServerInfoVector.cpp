#define INCL_WIN
#include <os2.h>

#include <sstream>

#include "kscp.h"

#include "ServerInfoVector.h"

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

