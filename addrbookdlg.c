/*
 * Copyright (c) 2012 by KO Myung-Hun <komh@chollian.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kscp.h"

#define KSCP_PRF_KEY_SERVER_COUNT "SERVER_COUNT"
#define KSCP_PRF_KEY_SERVER_BASE  "SERVER"

#define abMessageBox( hwnd, szMsg, fl ) \
    WinMessageBox( HWND_DESKTOP, hwnd, szMsg, "Address Book", ID_MSGBOX, fl )

typedef struct _SERVERINFOLISTITEM SERVERINFOLISTITEM, *PSERVERINFOLISTITEM;

struct _SERVERINFOLISTITEM
{
    LONG                lIndex;
    PSERVERINFO         psi;
    PSERVERINFOLISTITEM psiliNext;
};

typedef struct _SERVERINFOLIST SERVERINFOLIST, *PSERVERINFOLIST;

struct _SERVERINFOLIST
{
    LONG                lCount;
    PSERVERINFOLISTITEM psiliStart;
    PSERVERINFOLISTITEM psiliEnd;
};

typedef struct _ADDRBOOKDLGDATA
{
    HWND            hwndDlg;
    PSERVERINFOLIST psil;
    PSERVERINFO     psiResult;
} ADDRBOOKDLGDATA, *PADDRBOOKDLGDATA;

static PSERVERINFOLIST silCreate( void )
{
    PSERVERINFOLIST psil;

    psil = calloc( 1, sizeof( *psil ));

    return psil;
}

static void silDestroy( PSERVERINFOLIST psil )
{
    PSERVERINFOLISTITEM psili, psiliNext;

    for( psili = psil->psiliStart; psili; psili = psiliNext )
    {
        psiliNext = psili->psiliNext;

        free( psili->psi );
        free( psili );
    }

    free( psil );
}

static PSERVERINFO silSearch( PSERVERINFOLIST psil, LONG lIndex )
{
    PSERVERINFOLISTITEM psili;

    for( psili = psil->psiliStart; psili; psili = psili->psiliNext )
    {
        if( psili->lIndex == lIndex )
            return psili->psi;
    }

    return NULL;
}

static void silAdd( PSERVERINFOLIST psil, PSERVERINFO psi )
{
    PSERVERINFOLISTITEM psiliNew;

    psiliNew = calloc( 1, sizeof( *psiliNew ));
    psiliNew->psi = malloc( sizeof( *psiliNew->psi ));
    memcpy( psiliNew->psi, psi, sizeof( *psiliNew->psi ));

    if( !psil->psiliStart )
        psil->psiliStart = psil->psiliEnd = psiliNew;
    else
    {
        psiliNew->lIndex = psil->psiliEnd->lIndex + 1;
        psil->psiliEnd->psiliNext = psiliNew;
        psil->psiliEnd = psiliNew;
    }

    psil->lCount++;
}

static void silRemove( PSERVERINFOLIST psil, LONG lIndex )
{
    PSERVERINFOLISTITEM psili, psiliPrev = NULL;

    for( psili = psil->psiliStart; psili; psili = psili->psiliNext )
    {
        if( psili->lIndex == lIndex )
        {
            PSERVERINFOLISTITEM psiliNext = psili->psiliNext;

            if( psiliPrev )
                psiliPrev->psiliNext = psili->psiliNext;
            else
                psil->psiliStart = psili->psiliNext;

            if( psili == psil->psiliEnd )
                psil->psiliEnd = psiliPrev;

            free( psili );

            for( psili = psiliNext; psili; psili = psili->psiliNext )
                psili->lIndex--;

            psil->lCount--;

            return;
        }

        psiliPrev = psili;
    }
}

static PSERVERINFO silQueryServer( PSERVERINFOLIST psil, LONG lIndex )
{
    PSERVERINFOLISTITEM psili;

    for( psili = psil->psiliStart; psili; psili = psili->psiliNext )
    {
        if( psili->lIndex == lIndex )
            break;
    }

    if( psili )
        return psili->psi;

    return NULL;
}

static PSERVERINFOLIST abInitServerList( void )
{
    PSERVERINFOLIST psilNew;
    PSERVERINFO     psi;
    ULONG           ulBufMax;
    LONG            lCount = 0;
    char            szKey[ 10 ];
    int             i;

    psilNew = silCreate();

    psi = malloc( sizeof( *psi ));

    ulBufMax = sizeof( lCount );
    PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SERVER_COUNT, &lCount, &ulBufMax );

    for( i = 0; i < lCount; i++ )
    {
        snprintf( szKey, sizeof( szKey ), "%s%d", KSCP_PRF_KEY_SERVER_BASE,
                  i );

        ulBufMax = sizeof( *psi );
        PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP, szKey, psi,
                             &ulBufMax );

        silAdd( psilNew, psi );
    }

    free( psi );

    return psilNew;
}

static void abDoneServerList( PSERVERINFOLIST psil )
{
    PSERVERINFO     psi;
    char            szKey[ 10 ];
    int             i;

    PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SERVER_COUNT, &psil->lCount,
                         sizeof( psil->lCount ));

    for( i = 0; i < psil->lCount; i++ )
    {
        snprintf( szKey, sizeof( szKey ), "%s%d", KSCP_PRF_KEY_SERVER_BASE,
                  i );

        psi = silQueryServer( psil, i );

        PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP, szKey, psi,
                             sizeof( *psi ));
    }

    silDestroy( psil );
}

static MRESULT wmInitDlg( HWND hwndDlg, MPARAM mp1, MPARAM mp2 )
{
    PADDRBOOKDLGDATA pabdd;

    SWP  swp;
    LONG cxScreen, cyScreen;

    PSERVERINFO psi;
    int         i;

    BOOL  fShow = FALSE;
    ULONG ulBufMax;

    WinQueryWindowPos( hwndDlg, &swp );
    cxScreen = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    cyScreen = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    swp.x = ( cxScreen - swp.cx ) / 2;
    swp.y = ( cyScreen - swp.cy ) / 2;

    WinSetWindowPos( hwndDlg, HWND_TOP, swp.x, swp.y, 0, 0, SWP_MOVE );

    pabdd = malloc( sizeof( *pabdd ));

    WinSetWindowPtr( hwndDlg, 0, pabdd );

    pabdd->hwndDlg = hwndDlg;

    pabdd->psil = abInitServerList();

    pabdd->psiResult = ( PSERVERINFO )mp2;

    for( i = 0;; i++ )
    {
        psi = silQueryServer( pabdd->psil, i );
        if( !psi )
            break;

        WinSendDlgItemMsg( hwndDlg, IDLB_ADDRBOOK_SERVERS, LM_INSERTITEM,
                           MPFROMSHORT( LIT_END ), MPFROMP( psi->szAddress ));
    }

    ulBufMax = sizeof( fShow );
    PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SHOW, &fShow, &ulBufMax );

    WinCheckButton( hwndDlg, IDCB_ADDRBOOK_SHOW, fShow );

    return MRFROMLONG( FALSE );
}

static MRESULT wmDestroy( HWND hwndDlg, MPARAM mp1, MPARAM mp2 )
{
    PADDRBOOKDLGDATA pabdd = WinQueryWindowPtr( hwndDlg, 0 );

    BOOL fShow;

    fShow = WinQueryButtonCheckstate( hwndDlg, IDCB_ADDRBOOK_SHOW );

    PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SHOW, &fShow, sizeof( fShow ));

    abDoneServerList( pabdd->psil );

    free( pabdd );

    return 0;
}

static MRESULT wmControl( HWND hwndDlg, MPARAM mp1, MPARAM mp2 )
{
    HWND hwndLB = WinWindowFromID( hwndDlg, IDLB_ADDRBOOK_SERVERS );

    if( SHORT2FROMMP( mp1 ) == LN_ENTER &&
        WinQueryLboxSelectedItem( hwndLB ) >= 0 )
        WinPostMsg( hwndDlg, WM_COMMAND, MPFROMSHORT( DID_OK ), 0 );

    return 0;
}

static void abdAdd( PADDRBOOKDLGDATA pabdd, HWND hwndLB )
{
    PSERVERINFO psi;

    psi = calloc( 1, sizeof( *psi ));

    if( getServerInfo( pabdd->hwndDlg, psi, FALSE ))
    {
        if( psi->szAddress[ 0 ])
        {
            WinInsertLboxItem( hwndLB, LIT_END, psi->szAddress );
            silAdd( pabdd->psil, psi );
        }
    }

    free( psi );
}

static inline LONG checkLboxIndex( PADDRBOOKDLGDATA pabdd, HWND hwndLB )
{
    LONG lIndex;

    lIndex = WinQueryLboxSelectedItem( hwndLB );
    if( lIndex < 0 )
    {
        abMessageBox( pabdd->hwndDlg, "Please selecte a server",
                      MB_OK | MB_INFORMATION );
    }

    return lIndex;
}

static void abdEdit( PADDRBOOKDLGDATA pabdd, HWND hwndLB )
{
    LONG lIndex;

    lIndex = checkLboxIndex( pabdd, hwndLB );
    if( lIndex >= 0 )
    {
        PSERVERINFO psi, psiTemp;

        psi = silSearch( pabdd->psil, lIndex );

        psiTemp = malloc( sizeof( *psiTemp ));
        memcpy( psiTemp, psi, sizeof( *psiTemp ));

        if( getServerInfo( pabdd->hwndDlg, psiTemp, TRUE ))
        {
            if( psiTemp->szAddress[ 0 ])
            {
                WinSetLboxItemText( hwndLB, lIndex, psiTemp->szAddress );

                memcpy( psi, psiTemp, sizeof( *psi ));
            }
        }

        free( psiTemp );
    }
}

static void abdRemove( PADDRBOOKDLGDATA pabdd, HWND hwndLB )
{
    LONG lIndex;

    lIndex = checkLboxIndex( pabdd, hwndLB );
    if( lIndex >= 0 )
    {
        if( abMessageBox( pabdd->hwndDlg, "Are you sure ?",
                          MB_YESNO | MB_QUERY ) == MBID_YES )
        {
            silRemove( pabdd->psil, lIndex );

            WinDeleteLboxItem( hwndLB, lIndex );
        }
    }
}

static void abdOk( PADDRBOOKDLGDATA pabdd, HWND hwndLB )
{
    LONG lIndex;

    lIndex = checkLboxIndex( pabdd, hwndLB );
    if( lIndex >= 0 )
    {
        PSERVERINFO psi;

        psi = silSearch( pabdd->psil, lIndex );
        memcpy( pabdd->psiResult, psi, sizeof( *pabdd->psiResult ));

        WinDismissDlg( pabdd->hwndDlg, DID_OK );
    }
}

static void abdCancel( PADDRBOOKDLGDATA pabdd, HWND hwndLB )
{
    WinDismissDlg( pabdd->hwndDlg, DID_CANCEL );
}

static MRESULT wmCommand( HWND hwndDlg, MPARAM mp1, MPARAM mp2 )
{
    PADDRBOOKDLGDATA pabdd = WinQueryWindowPtr( hwndDlg, 0 );

    HWND hwndLB = WinWindowFromID( hwndDlg, IDLB_ADDRBOOK_SERVERS );

    switch( SHORT1FROMMP( mp1 ))
    {
        case IDPB_ADDRBOOK_ADD :
            abdAdd( pabdd, hwndLB );
            break;

        case IDPB_ADDRBOOK_EDIT :
            abdEdit( pabdd, hwndLB );
            break;

        case IDPB_ADDRBOOK_REMOVE :
            abdRemove( pabdd, hwndLB );
            break;

        case DID_OK :
            abdOk( pabdd, hwndLB );
            break;

        case DID_CANCEL :
            abdCancel( pabdd, hwndLB );
            break;
    }

    WinSetFocus( HWND_DESKTOP, hwndLB );

    return 0;
}

static MRESULT EXPENTRY abDlgProc( HWND hwndDlg, ULONG msg, MPARAM mp1,
                                   MPARAM mp2 )
{
    switch( msg )
    {
        case WM_INITDLG : return wmInitDlg( hwndDlg, mp1, mp2 );
        case WM_DESTROY : return wmDestroy( hwndDlg, mp1, mp2 );
        case WM_CONTROL : return wmControl( hwndDlg, mp1, mp2 );
        case WM_COMMAND : return wmCommand( hwndDlg, mp1, mp2 );
    }

    return WinDefDlgProc( hwndDlg, msg, mp1, mp2 );
}

BOOL abDlg( HWND hwndO, PSERVERINFO psiResult )
{
    return ( WinDlgBox( HWND_DESKTOP, hwndO, abDlgProc, 0, IDD_ADDRBOOK,
                        psiResult ) == DID_OK ) ? TRUE : FALSE;
}

BOOL getServerInfo( HWND hwndO, PSERVERINFO psi, BOOL fSet )
{
    HWND  hwndDlg;
    ULONG ulReply = DID_CANCEL;

    hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndO, WinDefDlgProc,
                          NULLHANDLE, IDD_OPEN, NULL );
    if( hwndDlg )
    {
        WinSendDlgItemMsg( hwndDlg, IDEF_OPEN_USERNAME, EM_SETTEXTLIMIT,
                           MPFROMSHORT( sizeof( psi->szUserName )), 0 );

        WinSendDlgItemMsg( hwndDlg, IDEF_OPEN_PASSWORD, EM_SETTEXTLIMIT,
                           MPFROMSHORT( sizeof( psi->szPassword )), 0 );

        WinSendDlgItemMsg( hwndDlg, IDEF_OPEN_DIRECTORY, EM_SETTEXTLIMIT,
                           MPFROMSHORT( sizeof( psi->szDir )), 0 );

        if( fSet )
        {
            WinSetDlgItemText( hwndDlg, IDCB_OPEN_ADDR, psi->szAddress );
            WinSetDlgItemText( hwndDlg, IDEF_OPEN_USERNAME, psi->szUserName );
            WinSetDlgItemText( hwndDlg, IDEF_OPEN_PASSWORD, psi->szPassword );
            WinSetDlgItemText( hwndDlg, IDEF_OPEN_DIRECTORY, psi->szDir );
        }

        ulReply = WinProcessDlg( hwndDlg );
        if( ulReply == DID_OK )
        {
            int len;

            WinQueryDlgItemText( hwndDlg, IDCB_OPEN_ADDR,
                                 sizeof( psi->szAddress ), psi->szAddress );

            WinQueryDlgItemText( hwndDlg, IDEF_OPEN_USERNAME,
                                 sizeof( psi->szUserName ), psi->szUserName );

            WinQueryDlgItemText( hwndDlg, IDEF_OPEN_PASSWORD,
                                 sizeof( psi->szPassword ), psi->szPassword );

            WinQueryDlgItemText( hwndDlg, IDEF_OPEN_DIRECTORY,
                                 sizeof( psi->szDir ), psi->szDir );

            len = strlen( psi->szDir );
            if( !len || psi->szDir[ len - 1 ] != '/')
                strcat( psi->szDir, "/");

            if( !psi->szAddress[ 0 ])
                ulReply = DID_CANCEL;
        }

        WinDestroyWindow( hwndDlg );
    }

    return ( ulReply == DID_OK ) ? TRUE : FALSE;
}

