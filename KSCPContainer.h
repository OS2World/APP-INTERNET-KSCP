#ifndef KSCP_CONTAINER_H
#define KSCP_CONTAINER_H

#include <os2.h>

#include <KContainer.h>

#include <libssh2_sftp.h>

typedef struct _KSCPRECORD
{
    MINIRECORDCORE mrc;
    PSZ            pszName;
    PVOID          pAttr;
} KSCPRECORD, *PKSCPRECORD;

class KSCPContainer : public KContainer< KSCPRECORD, true >
{
protected :
    virtual SHORT KSortCompare( PKSCPRECORD p1, PKSCPRECORD p2,
                                PVOID pStorage )
    {
        LIBSSH2_SFTP_ATTRIBUTES *pattr1, *pattr2;

        int rc;

        pattr1 = ( LIBSSH2_SFTP_ATTRIBUTES * )p1->pAttr;
        pattr2 = ( LIBSSH2_SFTP_ATTRIBUTES * )p2->pAttr;

        // directory first
        rc = LIBSSH2_SFTP_S_ISDIR( pattr2->permissions ) -
             LIBSSH2_SFTP_S_ISDIR( pattr1->permissions );

        if( rc == 0 )
            rc = strcmp(  p1->pszName, p2->pszName );

        return rc;
    }
};
#endif
