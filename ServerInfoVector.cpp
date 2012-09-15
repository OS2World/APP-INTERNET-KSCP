#define INCL_WIN
#include <os2.h>

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
    PSERVERINFO psi;
    ULONG       ulBufMax;
    LONG        lCount = 0;
    char        szKey[ 10 ];
    int         i;

    ulBufMax = sizeof( lCount );
    PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SERVER_COUNT, &lCount, &ulBufMax );

    for( i = 0; i < lCount; i++ )
    {
        snprintf( szKey, sizeof( szKey ), "%s%d",
                  KSCP_PRF_KEY_SERVER_BASE, i );

        psi = new SERVERINFO;

        ulBufMax = sizeof( *psi );
        PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP, szKey, psi,
                             &ulBufMax );

        Add( psi );
    }
}

void ServerInfoVector::Save()
{
    LONG lCount = _vtServerInfo.size();
    char szKey[ 10 ];

    PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SERVER_COUNT, &lCount,
                         sizeof( lCount ));

    for( int i = 0; i < lCount; ++i )
    {
        snprintf( szKey, sizeof( szKey ), "%s%d",
                  KSCP_PRF_KEY_SERVER_BASE, i );

        PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP, szKey,
                             QueryServer( i ), sizeof( SERVERINFO ));
    }
}

