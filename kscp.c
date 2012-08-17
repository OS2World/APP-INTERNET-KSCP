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

#define INCL_DOS
#define INCL_PM
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>

#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>

#include "kscp.h"

#define KSCP_PRF_KEY_DLDIR         "DownloadDir"

static void removeRecordAll( PKSCPDATA pkscp )
{
    PKSCPRECORD pkr, pkrNext;

    pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORD, NULL,
                      MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER ));
    for(; pkr; pkr = pkrNext )
    {
        pkrNext = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORD, pkr,
                              MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ));

        free( pkr->pszName );
        free( pkr->pAttr );

        WinSendMsg( pkscp->hwndCnr, CM_REMOVERECORD, &pkr,
                    MPFROM2SHORT( 1, CMA_FREE ));
    }

    WinSendMsg( pkscp->hwndCnr, CM_INVALIDATERECORD, 0,
                MPFROM2SHORT( 0, CMA_REPOSITION ));
}

static void kscpDisconnect( PKSCPDATA pkscp )
{
    if( pkscp->sock == -1 )
        return;

    removeRecordAll( pkscp );

    WinSendMsg( pkscp->hwndCnr, CM_REMOVEDETAILFIELDINFO, NULL,
                MPFROM2SHORT( 0, CMA_FREE | CMA_INVALIDATE ));

    WinDestroyWindow( pkscp->hwndCnr );

    libssh2_sftp_shutdown( pkscp->sftp_session );
    pkscp->sftp_session = NULL;

    libssh2_session_disconnect( pkscp->session, "Normal Shutdown, Thank you for playing");
    libssh2_session_free( pkscp->session );
    pkscp->session = NULL;

    close( pkscp->sock );
    pkscp->sock = -1;

    free( pkscp->pszCurDir );
    pkscp->pszCurDir = NULL;
}

static SHORT EXPENTRY fileCompare( PKSCPRECORD p1, PKSCPRECORD p2,
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
        rc = strcmp( p1->pszName, p2->pszName );

    return rc;
}

static BOOL readDir( PKSCPDATA pkscp, const char *dir )
{
    LIBSSH2_SFTP_HANDLE *sftp_handle;
    PKSCPRECORD          pkr;
    RECORDINSERT         ri;
    ULONG                ulStyle;
    int                  rc;

    fprintf( stderr, "libssh2_sftp_opendir()!\n");
    /* Request a dir listing via SFTP */
    sftp_handle = libssh2_sftp_opendir( pkscp->sftp_session, dir );

    if (!sftp_handle)
    {
        WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Unable to open dir with SFTP", "OpenDir",
                       ID_MSGBOX, MB_OK | MB_ERROR );

        return FALSE;
    }
    fprintf( stderr, "libssh2_sftp_opendir() is done, now receive listing!\n");
    while( 1 )
    {
        char mem[ 512 ];
        char longentry[ 512 ];
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        /* loop until we fail */
        rc = libssh2_sftp_readdir_ex( sftp_handle, mem, sizeof( mem ),
                                      longentry, sizeof( longentry ), &attrs );
        if( rc <= 0 )
            break;

        if( strcmp( mem, ".")) {
            HPOINTER hptrIcon = pkscp->hptrDefaultFile;

            pkr = WinSendMsg( pkscp->hwndCnr, CM_ALLOCRECORD,
                              MPFROMLONG( CB_EXTRA_KSCPRECORD ),
                              MPFROMLONG( 1 ));

            if(( attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS ) &&
               LIBSSH2_SFTP_S_ISDIR( attrs.permissions ))
                hptrIcon = pkscp->hptrDefaultFolder;

            pkr->pszName       = strdup( mem );
            pkr->pAttr         = malloc( sizeof( attrs ));
            memcpy( pkr->pAttr, &attrs, sizeof( attrs ));
            //pkr->mrc.cb        = sizeof( KSCPRECORD );
            pkr->mrc.hptrIcon  = hptrIcon;
            pkr->mrc.pszIcon   = pkr->pszName;

            ri.cb                = sizeof( RECORDINSERT );
            ri.pRecordOrder      = ( PRECORDCORE )CMA_END;
            ri.pRecordParent     = NULL;
            ri.zOrder            = ( USHORT )CMA_TOP;
            ri.fInvalidateRecord = FALSE;
            ri.cRecordsInsert    = 1;

            WinSendMsg( pkscp->hwndCnr, CM_INSERTRECORD, pkr, &ri );
        }
    }

    libssh2_sftp_closedir( sftp_handle );

    WinSendMsg( pkscp->hwndCnr, CM_INVALIDATERECORD, 0,
                MPFROM2SHORT( 0, CMA_REPOSITION ));

    // Change a selection type to a single selection
    ulStyle = WinQueryWindowULong( pkscp->hwndCnr, QWL_STYLE );
    ulStyle &= ~CCS_EXTENDSEL;
    ulStyle |=  CCS_SINGLESEL;
    WinSetWindowULong( pkscp->hwndCnr, QWL_STYLE, ulStyle );

    // Select the first item and clear emphasis of other item
    WinSendMsg( pkscp->hwndCnr, CM_SETRECORDEMPHASIS,
                WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORD, NULL,
                            MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER )),
                MPFROM2SHORT( TRUE, CRA_CURSORED | CRA_SELECTED ));

    // Change a selection type to a extend selection
    ulStyle = WinQueryWindowULong( pkscp->hwndCnr, QWL_STYLE );
    ulStyle &= ~CCS_SINGLESEL;
    ulStyle |=  CCS_EXTENDSEL;
    WinSetWindowULong( pkscp->hwndCnr, QWL_STYLE, ulStyle );

    // Scroll to the top
    WinSendMsg( pkscp->hwndCnr, WM_CHAR,
                MPFROMSHORT( KC_VIRTUALKEY ), MPFROM2SHORT( 0, VK_HOME ));

    return TRUE;
}

static BOOL kscpConnect( PKSCPDATA pkscp, PSERVERINFO psi )
{
    struct sockaddr_in sin;
    const char        *fingerprint;
    char               szFingerprint[ 80 ];
    int                auth_pw = 1;
    struct hostent    *host;
    PFIELDINFO         pfi, pfiStart;
    FIELDINFOINSERT    fii;
    CNRINFO            ci;
    int                i;
    int                rc;
    RECTL              rcl;

    if( pkscp->sock != -1 )
        kscpDisconnect( pkscp );

    pkscp->pszCurDir = strdup( psi->szDir );

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    pkscp->sock = socket( AF_INET, SOCK_STREAM, 0 );

    host = gethostbyname( psi->szAddress );

    sin.sin_family = AF_INET;
    sin.sin_port = htons( 22 );
    sin.sin_addr.s_addr = *( u_long * )host->h_addr;
    if( connect( pkscp->sock, ( struct sockaddr * )( &sin ),
                 sizeof( struct sockaddr_in )) != 0 )
    {
        fprintf(stderr, "failed to connect!\n");

        goto exit_close_socket;
    }

    /* Create a session instance
     */
    pkscp->session = libssh2_session_init();
    if( !pkscp->session )
        goto exit_close_socket;

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake( pkscp->session, pkscp->sock );
    if(rc)
    {
        fprintf( stderr, "Failure establishing SSH session: %d\n", rc );

        goto exit_session_free;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash( pkscp->session,
                                        LIBSSH2_HOSTKEY_HASH_SHA1 );
    for( i = 0; i < 20; i++ )
        sprintf( szFingerprint + i * 3, "%02X ",
                 ( unsigned char )fingerprint[ i ]);

    WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szFingerprint,
                   "Fingerprint", ID_MSGBOX, MB_OK | MB_INFORMATION );

    if( auth_pw )
    {
        /* We could authenticate via password */
        if( libssh2_userauth_password( pkscp->session, psi->szUserName,
                                       psi->szPassword ))
        {
            printf("Authentication by password failed.\n");

            goto exit_session_disconnect;
        }
    }
    else
    {
        /* Or by public key */
        if( libssh2_userauth_publickey_fromfile( pkscp->session,
                            psi->szUserName,
                            "/home/username/.ssh/id_rsa.pub",
                            "/home/username/.ssh/id_rsa",
                            psi->szPassword ))
        {
            printf("\tAuthentication by public key failed\n");

            goto exit_session_disconnect;
        }
    }

    fprintf( stderr, "libssh2_sftp_init()!\n");
    pkscp->sftp_session = libssh2_sftp_init( pkscp->session );

    if( !pkscp->sftp_session )
    {
        fprintf( stderr, "Unable to init SFTP session\n");

        goto exit_session_disconnect;
    }

    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking( pkscp->session, 1 );

    pkscp->hwndCnr = WinCreateWindow( pkscp->hwnd, WC_CONTAINER, NULL,
                                      CCS_AUTOPOSITION | CCS_READONLY |
                                      CCS_EXTENDSEL | CCS_MINIRECORDCORE |
                                      CCS_MINIICONS,
                                      0, 0, 0, 0,
                                      pkscp->hwnd, HWND_TOP, IDC_CONTAINER,
                                      NULL, NULL );

    pfi = pfiStart = WinSendMsg( pkscp->hwndCnr, CM_ALLOCDETAILFIELDINFO,
                                 MPFROMLONG( 2 ), NULL );

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_BITMAPORICON | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR;
    pfi->flTitle    = CFA_CENTER;
    pfi->pTitleData = "Icon";
    pfi->offStruct  = FIELDOFFSET(KSCPRECORD, mrc.hptrIcon );
    pfi             = pfi->pNextFieldInfo;

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR;
    pfi->flTitle    = CFA_CENTER;
    pfi->pTitleData = "Name";
    pfi->offStruct  = FIELDOFFSET(KSCPRECORD, pszName);

    fii.cb                   = ( ULONG )( sizeof( FIELDINFOINSERT ));
    fii.pFieldInfoOrder      = (PFIELDINFO)CMA_FIRST;
    fii.cFieldInfoInsert     = 2;
    fii.fInvalidateFieldInfo = TRUE;

    WinSendMsg( pkscp->hwndCnr, CM_INSERTDETAILFIELDINFO,
                MPFROMP( pfiStart ), MPFROMP( &fii ));

    ci.pSortRecord  = fileCompare;
    ci.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES | CA_DRAWICON |
                      CV_MINI;
    ci.slBitmapOrIcon.cx = 0;
    ci.slBitmapOrIcon.cy = ci.slBitmapOrIcon.cx;

    WinSendMsg( pkscp->hwndCnr, CM_SETCNRINFO, MPFROMP( &ci ),
                MPFROMLONG( CMA_PSORTRECORD | CMA_FLWINDOWATTR |
                            CMA_SLBITMAPORICON ));

    if( !readDir( pkscp, psi->szDir ))
        goto exit_destroy_window;

    WinQueryWindowRect( pkscp->hwnd, &rcl );
    WinSetWindowPos( pkscp->hwndCnr, HWND_TOP,
                     rcl.xLeft, rcl.yBottom,
                     rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                     SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_SHOW );
    WinSetFocus( HWND_DESKTOP, pkscp->hwndCnr );

    return TRUE;

exit_destroy_window :
    WinDestroyWindow( pkscp->hwndCnr );
    pkscp->hwndCnr = NULLHANDLE;

    libssh2_sftp_shutdown( pkscp->sftp_session );
    pkscp->sftp_session = NULL;

exit_session_disconnect :
    libssh2_session_disconnect( pkscp->session, "Abnormal Shutdown");

exit_session_free :
    libssh2_session_free( pkscp->session );
    pkscp->session = NULL;

exit_close_socket :
    close( pkscp->sock );
    pkscp->sock = -1;

    free( pkscp->pszCurDir );

    return FALSE;
}

static MRESULT wmCreate( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp;
    char szStr[ 256 ];

    pkscp = calloc( 1, sizeof( *pkscp ));
    WinSetWindowPtr( hwnd, 0, pkscp );

    pkscp->hwnd = hwnd;

    pkscp->sock = -1;

    if( DosLoadModule( szStr, sizeof( szStr ), "pmwp", &pkscp->hmodPMWP ))
    {
        pkscp->hmodPMWP = NULLHANDLE;

        pkscp->hptrDefaultFile   = WinQuerySysPointer( HWND_DESKTOP,
                                                       SPTR_FILE, FALSE );

        pkscp->hptrDefaultFolder = WinQuerySysPointer( HWND_DESKTOP,
                                                       SPTR_FOLDER, FALSE );
    }
    else
    {
        pkscp->hptrDefaultFile   = WinLoadPointer( HWND_DESKTOP,
                                                   pkscp->hmodPMWP, 24 );
        pkscp->hptrDefaultFolder = WinLoadPointer( HWND_DESKTOP,
                                                   pkscp->hmodPMWP, 26 );
    }

    pkscp->hwndPopup = WinLoadMenu( hwnd, 0, IDM_KSCP_POPUP );

    PrfQueryProfileString( HINI_USERPROFILE, KSCP_PRF_APP, KSCP_PRF_KEY_DLDIR,
                           "", szStr, sizeof( szStr ));

    pkscp->pszDlDir = strdup( szStr );

    return 0;
}

static MRESULT wmDestroy( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp = WinQueryWindowPtr( hwnd, 0 );

    kscpDisconnect( pkscp );

    PrfWriteProfileString( HINI_USERPROFILE, KSCP_PRF_APP, KSCP_PRF_KEY_DLDIR,
                           pkscp->pszDlDir );

    free( pkscp->pszDlDir );

    if( pkscp->hmodPMWP )
    {
        WinDestroyPointer( pkscp->hptrDefaultFile );
        WinDestroyPointer( pkscp->hptrDefaultFolder );

        DosFreeModule( pkscp->hmodPMWP );
    }

    free( pkscp );

    return 0;
}

static MRESULT wmSize( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp = WinQueryWindowPtr( hwnd, 0 );

    WinSetWindowPos( pkscp->hwndCnr, HWND_TOP, 0, 0,
                     SHORT1FROMMP( mp2 ), SHORT2FROMMP( mp2 ), SWP_SIZE );

    return MRFROMLONG( TRUE );
}

static BOOL fileOpen( PKSCPDATA pkscp )
{
    PSERVERINFO psi;
    ULONG       rc = FALSE;

    psi = calloc( 1, sizeof( *psi ));

    if( getServerInfo( pkscp->hwnd, psi, FALSE ))
        rc = kscpConnect( pkscp, psi );

    free( psi );

    return rc;
}

static void fileClose( PKSCPDATA pkscp )
{
    kscpDisconnect( pkscp );
}

static BOOL fileAddrBook( PKSCPDATA pkscp )
{
    PSERVERINFO psi;
    ULONG       rc = FALSE;

    psi = calloc( 1, sizeof( *psi ));

    if( abDlg( pkscp->hwnd, psi ))
        rc = kscpConnect( pkscp, psi );

    free( psi );

    return rc;
}

static void fileDlDir( PKSCPDATA pkscp )
{
    FILEDLG fd;

    memset( &fd, 0, sizeof( fd ));
    fd.pszTitle = "Choose a download directory";
    strcpy( fd.szFullFile, pkscp->pszDlDir );

    if( WinDirDlg( HWND_DESKTOP, pkscp->hwnd, &fd ) &&
        fd.lReturn == DID_OK )
    {
        free( pkscp->pszDlDir );

        pkscp->pszDlDir = strdup( fd.szFullFile );
    }
}

#define BUF_SIZE    ( 1024 * 4 )

static int download( PKSCPDATA pkscp, PKSCPRECORD pkr )
{
    LIBSSH2_SFTP_HANDLE     *sftp_handle;
    char                     sftppath[ 512 ];
    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    FILE             *fp;
    char             *buf;
    libssh2_uint64_t  size;
    char              szMsg[ 512 ];

    struct timeval tv1, tv2;
    long long      diffTime;

    snprintf( sftppath, sizeof( sftppath ), "%s%s",
              pkscp->pszCurDir, pkr->pszName );
    sftp_handle = libssh2_sftp_open( pkscp->sftp_session, sftppath,
                                     LIBSSH2_FXF_READ, 0 );
    if( !sftp_handle )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot open %s", sftppath );

        WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Open", ID_MSGBOX,
                       MB_OK | MB_ERROR );

        return 1;
    }

    pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )pkr->pAttr;

    buf = malloc( BUF_SIZE );
    _makepath( buf, NULL, pkscp->pszDlDir, pkr->pszName, NULL );

    fp = fopen( buf, "wb");

    if( pattr->filesize )
    {
        int nRead;

        for( size = diffTime = 0; !pkscp->fCanceled; )
        {
            sprintf( szMsg, "%lld KB of %lld KB (%lld%%)",
                     size / 1024, pattr->filesize / 1024,
                     size * 100 / pattr->filesize );

            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_STATUS, szMsg );

            gettimeofday( &tv1, NULL );
            /* read in a loop until we block */
            nRead = libssh2_sftp_read( sftp_handle, buf, BUF_SIZE );
            gettimeofday( &tv2, NULL );

            diffTime += ( tv2.tv_sec * 1000000LL + tv2.tv_usec ) -
                        ( tv1.tv_sec * 1000000LL + tv2.tv_usec );

            if( nRead <= 0 )
                break;

            fwrite( buf, nRead, 1, fp );

            size += nRead;

            if( diffTime )
            {
                sprintf( szMsg, "%lld KB/s",
                         ( size * 1000000LL / 1024 ) / diffTime );
                WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_SPEED,
                                   szMsg );
            }
        }
    }

    fclose( fp );

    free( buf );

    libssh2_sftp_close( sftp_handle );

    return 0;
}

static int checkDlFiles( PKSCPDATA pkscp )
{
    PKSCPRECORD pkr;
    int         count;

    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                      MPFROMLONG( CMA_FIRST ),
                      MPFROMLONG( CRA_SELECTED ));

    for( count = 0; pkr; )
    {
        pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )pkr->pAttr;
        if( LIBSSH2_SFTP_S_ISREG( pattr->permissions ))
            count++;

        pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                          MPFROMP( pkr ), MPFROMLONG( CRA_SELECTED ));
    }

    return count;
}

static void downloadThread( void *arg )
{
    PKSCPDATA   pkscp = ( PKSCPDATA )arg;

    HAB hab;
    HMQ hmq;

    PKSCPRECORD pkr;

    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    char szMsg[ 100 ];
    int  count;
    int  i;

    pkscp->fBusy = TRUE;

    // to use WinSendMsg()
    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0);

    count = checkDlFiles( pkscp );

    pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                      MPFROMLONG( CMA_FIRST ), MPFROMLONG( CRA_SELECTED ));

    for( i = 0; pkr && !pkscp->fCanceled;  )
    {
        pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )pkr->pAttr;
        if( LIBSSH2_SFTP_S_ISREG( pattr->permissions ))
        {
            i++;
            sprintf( szMsg, "%d of %d", i, count );
            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_INDEX,
                               szMsg );
            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_FILENAME,
                               pkr->pszName );

            download( pkscp, pkr );
        }

        pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                          MPFROMP( pkr ),
                          MPFROMLONG( CRA_SELECTED ));
    }

    WinDismissDlg( pkscp->hwndDlg, pkscp->fCanceled ? DID_CANCEL : DID_OK );

    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    pkscp->fBusy = FALSE;
}

static int kscpDownload( PKSCPDATA pkscp )
{
    TID   tid;
    char  szMsg[ 100 ];
    ULONG ulReply;

    if( pkscp->fBusy )
    {
        WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Session is busy\nTry again, later", "Download",
                       ID_MSGBOX, MB_OK | MB_ERROR );

        return 1;
    }

    if( !checkDlFiles( pkscp ))
    {
        WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Files not selected", "Download",
                       ID_MSGBOX, MB_OK | MB_ERROR );

        return 1;
    }

    pkscp->fCanceled = FALSE;

    pkscp->hwndDlg = WinLoadDlg( HWND_DESKTOP, pkscp->hwnd, WinDefDlgProc,
                                 0, IDD_DOWNLOAD, pkscp );

    tid = _beginthread( downloadThread, NULL, 1024 * 1024, pkscp );

    ulReply = WinProcessDlg( pkscp->hwndDlg );
    if( ulReply == DID_CANCEL )
        pkscp->fCanceled = TRUE;

    WinDestroyWindow( pkscp->hwndDlg );

    pkscp->hwndDlg = NULLHANDLE;

    sprintf( szMsg, "Download %s",
             ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Download",
                   ID_MSGBOX, MB_OK | MB_INFORMATION );

    return 0;
}

static int upload( PKSCPDATA pkscp, const char *pszName )
{
    LIBSSH2_SFTP_HANDLE *sftp_handle;
    char                 sftppath[ 512 ];

    FILE *fp;
    off_t size, fileSize;
    char *buf;
    char  szMsg[ 512 ];

    struct stat statbuf;

    struct timeval tv1, tv2;
    long long      diffTime;

    int rc = 1;

    if( stat( pszName, &statbuf ) < 0 || !S_ISREG( statbuf.st_mode ))
    {
        snprintf( szMsg, sizeof( szMsg ),
                 "Ooops... This is not a file. Ignored.\n%s", pszName );

        WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Upload", ID_MSGBOX,
                       MB_OK | MB_ERROR );

        return rc;
    }

    fp = fopen( pszName, "rb");
    if( !fp )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot open %s", pszName );

        WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Upload", ID_MSGBOX,
                       MB_OK | MB_ERROR );

        return rc;
    }

    fseeko( fp, 0, SEEK_END );
    fileSize = ftello( fp );
    fseeko( fp, 0, SEEK_SET );

    snprintf( sftppath, sizeof( sftppath ), "%s%s",
              pkscp->pszCurDir, strrchr( pszName, '\\') + 1 );
    sftp_handle = libssh2_sftp_open( pkscp->sftp_session, sftppath,
                                     LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT,
                                     LIBSSH2_SFTP_S_IRUSR |
                                     LIBSSH2_SFTP_S_IWUSR |
                                     LIBSSH2_SFTP_S_IRGRP |
                                     LIBSSH2_SFTP_S_IROTH );
    if( !sftp_handle )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot create %s", sftppath );

        WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Upload", ID_MSGBOX,
                       MB_OK | MB_ERROR );

        goto exit_fclose;
    }

    buf = malloc( BUF_SIZE );

    if( fileSize )
    {
        int         nRead, nWrite;
        const char *ptr;

        for( size = diffTime = 0; !pkscp->fCanceled; )
        {
            sprintf( szMsg, "%lld KB of %lld KB (%lld%%)",
                     size / 1024, fileSize / 1024,
                     size * 100 / fileSize );

            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_STATUS, szMsg );

            nRead = fread( buf, 1, BUF_SIZE, fp );
            if( nRead <= 0 )
                break;

            size += nRead;

            for( ptr = buf; nRead; )
            {
                gettimeofday( &tv1, NULL );
                /* write data in a loop until we block */
                nWrite = libssh2_sftp_write( sftp_handle, ptr, nRead );
                gettimeofday( &tv2, NULL );

                diffTime += ( tv2.tv_sec * 1000000LL + tv2.tv_usec ) -
                            ( tv1.tv_sec * 1000000LL + tv2.tv_usec );

                if( nWrite < 0 )
                    break;

                ptr   += nWrite;
                nRead -= nWrite;
            }

            if( nRead )
            {
                snprintf( szMsg, sizeof( szMsg ),
                          "Ooops... Error occurs while uploading\n%s",
                          pszName );
                WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Upload",
                               ID_MSGBOX, MB_OK | MB_ERROR );
                goto exit_free;
            }

            if( diffTime )
            {
                sprintf( szMsg, "%lld KB/s",
                         ( size * 1000000LL / 1024 ) / diffTime );
                WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_SPEED,
                                   szMsg );
            }
        }
    }

    libssh2_sftp_close( sftp_handle );

    rc = 0;

exit_free :
    free( buf );

exit_fclose :
    fclose( fp );

    return rc;
}

typedef struct tagUPLOADDATA
{
    PKSCPDATA pkscp;
    APSZ      apszName;
    PAPSZ     papszList;
    ULONG     ulCount;
} UPLOADDATA, *PUPLOADDATA;

static void uploadThread( void *arg )
{
    PKSCPDATA pkscp      =  (( PUPLOADDATA )arg )->pkscp;
    PAPSZ     papszList  =  (( PUPLOADDATA )arg )->papszList;
    ULONG     ulCount    =  (( PUPLOADDATA )arg )->ulCount;

    HAB hab;
    HMQ hmq;

    char szMsg[ 100 ];
    int  i;

    pkscp->fBusy = TRUE;

    // to use WinSendMsg()
    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0);

    if( !papszList )
        papszList = &(( PUPLOADDATA )arg )->apszName;

    for( i = 0; i < ulCount && !pkscp->fCanceled; i++ )
    {
        sprintf( szMsg, "%d of %ld", i + 1, ulCount );
        WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_INDEX, szMsg );
        WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_FILENAME,
                           (*papszList)[ i ]);
        upload( pkscp, (*papszList)[ i ]);
    }

    WinDismissDlg( pkscp->hwndDlg, pkscp->fCanceled ? DID_CANCEL : DID_OK );

    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    pkscp->fBusy = FALSE;
}

static void refresh( PKSCPDATA pkscp )
{
    removeRecordAll( pkscp );

    readDir( pkscp, pkscp->pszCurDir );
}

static int kscpUpload( PKSCPDATA pkscp )
{
    FILEDLG    fd;
    UPLOADDATA ud;
    TID        tid;
    char       szMsg[ 100 ];
    ULONG      ulReply;

    if( pkscp->fBusy )
    {
        WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Session is busy\nTry again, later", "Upload",
                       ID_MSGBOX, MB_OK | MB_ERROR );

        return 1;
    }

    memset( &fd, 0, sizeof( fd ));
    fd.cbSize = sizeof( fd );
    fd.fl     = FDS_CENTER | FDS_MULTIPLESEL | FDS_OPEN_DIALOG;
    fd.pszTitle = "Upload";

    if( !WinFileDlg( HWND_DESKTOP, pkscp->hwnd, &fd ) ||
        fd.lReturn != DID_OK )
        return 1;

    pkscp->fCanceled = FALSE;

    pkscp->hwndDlg = WinLoadDlg( HWND_DESKTOP, pkscp->hwnd, WinDefDlgProc,
                                 0, IDD_DOWNLOAD, pkscp );

    WinSetWindowText( pkscp->hwndDlg, "Upload");

    ud.pkscp         = pkscp;
    ud.apszName[ 0 ] = fd.szFullFile;
    ud.papszList     = fd.papszFQFilename;
    ud.ulCount       = fd.ulFQFCount;

    tid = _beginthread( uploadThread, NULL, 1024 * 1024, &ud );

    ulReply = WinProcessDlg( pkscp->hwndDlg );
    if( ulReply == DID_CANCEL )
        pkscp->fCanceled = TRUE;

    WinDestroyWindow( pkscp->hwndDlg );

    pkscp->hwndDlg = NULLHANDLE;

    WinFreeFileDlgList( fd.papszFQFilename );

    sprintf( szMsg, "Upload %s",
             ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Upload",
                   ID_MSGBOX, MB_OK | MB_INFORMATION );

    refresh( pkscp );

    return 0;
}

static int delete( PKSCPDATA pkscp, PKSCPRECORD pkr )
{
    char sftppath[ 512 ];
    char szMsg[ 512 ];

    snprintf( sftppath, sizeof( sftppath ), "%s%s",
              pkscp->pszCurDir, pkr->pszName );

    if( libssh2_sftp_unlink( pkscp->sftp_session, sftppath ))
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot delete %s", sftppath );

        WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Delete", ID_MSGBOX,
                       MB_OK | MB_ERROR );

        return 1;
    }

    return 0;
}

static void deleteThread( void *arg )
{
    PKSCPDATA   pkscp = ( PKSCPDATA )arg;

    HAB hab;
    HMQ hmq;

    PKSCPRECORD pkr;

    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    char szMsg[ 100 ];
    int  count;
    int  i;

    pkscp->fBusy = TRUE;

    // to use WinSendMsg()
    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0);

    count = checkDlFiles( pkscp );

    pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                      MPFROMLONG( CMA_FIRST ), MPFROMLONG( CRA_SELECTED ));

    for( i = 0; pkr && !pkscp->fCanceled;  )
    {
        pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )pkr->pAttr;
        if( LIBSSH2_SFTP_S_ISREG( pattr->permissions ))
        {
            i++;
            sprintf( szMsg, "%d of %d", i, count );
            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_INDEX,
                               szMsg );
            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_FILENAME,
                               pkr->pszName );

            delete( pkscp, pkr );
        }

        pkr = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                          MPFROMP( pkr ),
                          MPFROMLONG( CRA_SELECTED ));
    }

    WinDismissDlg( pkscp->hwndDlg, pkscp->fCanceled ? DID_CANCEL : DID_OK );

    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    pkscp->fBusy = FALSE;
}

static int kscpDelete( PKSCPDATA pkscp )
{
    TID   tid;
    char  szMsg[ 100 ];
    ULONG ulReply;

    if( pkscp->fBusy )
    {
        WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Session is busy\nTry again, later", "Delete",
                       ID_MSGBOX, MB_OK | MB_ERROR );

        return 1;
    }

    if( !checkDlFiles( pkscp ))
    {
        WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Files not selected", "Delete",
                       ID_MSGBOX, MB_OK | MB_ERROR );

        return 1;
    }

    if( WinMessageBox( HWND_DESKTOP, pkscp->hwnd,
                       "Are you sure to delete ?", "Delete",
                       ID_MSGBOX, MB_YESNO | MB_ICONQUESTION ) == MBID_NO )
        return 1;

    pkscp->fCanceled = FALSE;

    pkscp->hwndDlg = WinLoadDlg( HWND_DESKTOP, pkscp->hwnd, WinDefDlgProc,
                                 0, IDD_DOWNLOAD, pkscp );

    WinSetWindowText( pkscp->hwndDlg, "Delete");
    WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_STATUS, "");
    WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_SPEED, "");

    tid = _beginthread( deleteThread, NULL, 1024 * 1024, pkscp );

    ulReply = WinProcessDlg( pkscp->hwndDlg );
    if( ulReply == DID_CANCEL )
        pkscp->fCanceled = TRUE;

    WinDestroyWindow( pkscp->hwndDlg );

    pkscp->hwndDlg = NULLHANDLE;

    sprintf( szMsg, "Delete %s",
             ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Delete",
                   ID_MSGBOX, MB_OK | MB_INFORMATION );

    refresh( pkscp );

    return 0;
}

static MRESULT wmCommand( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp = WinQueryWindowPtr( hwnd, 0 );

    switch( SHORT1FROMMP( mp1 ))
    {
        case IDM_FILE_OPEN  :
            fileOpen( pkscp );
            break;

        case IDM_FILE_ADDRBOOK :
            fileAddrBook( pkscp );
            break;

        case IDM_FILE_CLOSE :
            fileClose( pkscp );
            break;

        case IDM_FILE_DLDIR :
            fileDlDir( pkscp );
            break;

        case IDM_FILE_EXIT  :
            WinPostMsg( hwnd, WM_QUIT, 0, 0 );
            break;

        case IDM_KSCP_DOWNLOAD :
            kscpDownload( pkscp );
            break;

        case IDM_KSCP_UPLOAD :
            kscpUpload( pkscp );
            break;

        case IDM_KSCP_DELETE :
            kscpDelete( pkscp );
            break;
    }

    return 0;
}

static MRESULT wmPaint( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    HPS   hps;
    RECTL rcl;

    hps = WinBeginPaint( hwnd, NULLHANDLE, &rcl);

    WinFillRect( hps, &rcl, SYSCLR_WINDOW );

    WinEndPaint( hps );

    return 0;
}

static void processEnter( PKSCPDATA pkscp, PKSCPRECORD pkr )
{
    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )pkr->pAttr;
    if( LIBSSH2_SFTP_S_ISDIR( pattr->permissions ))
    {
        char *pszNewDir = NULL;

        if( strcmp( pkr->pszName, ".."))
            asprintf( &pszNewDir, "%s%s/", pkscp->pszCurDir, pkr->pszName );
        else if( pkscp->pszCurDir[ 1 ]) // not root ?
        {
            // remove the last '/'
            pkscp->pszCurDir[ strlen( pkscp->pszCurDir ) - 1 ] = '\0';

            // remove the last directory part
            *( strrchr( pkscp->pszCurDir, '/' ) + 1 ) = '\0';

            pszNewDir = strdup( pkscp->pszCurDir );
        }

        if( pszNewDir )
        {
            removeRecordAll( pkscp );

            if( !readDir( pkscp, pszNewDir ))
            {
                readDir( pkscp, pkscp->pszCurDir );

                free( pszNewDir );
            }
            else
            {
                free( pkscp->pszCurDir );

                pkscp->pszCurDir = pszNewDir;
            }
        }
    }
    else
        kscpDownload( pkscp );
}

static MRESULT wmControl( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp = WinQueryWindowPtr( hwnd, 0 );

    switch( SHORT2FROMMP( mp1 ))
    {
        case CN_CONTEXTMENU :
        {
            POINTL ptl;
            ULONG  fs = PU_NONE | PU_KEYBOARD | PU_MOUSEBUTTON1 |
                        PU_HCONSTRAIN | PU_VCONSTRAIN;

            WinQueryPointerPos( HWND_DESKTOP, &ptl );
            WinMapWindowPoints( HWND_DESKTOP, hwnd, &ptl, 1 );

            WinPopupMenu( hwnd, hwnd, pkscp->hwndPopup, ptl.x, ptl.y, 0, fs );
            break;
        }

        case CN_ENTER :
            // pRecord is NULL if a heading of container is double-clicked
            if((( PNOTIFYRECORDENTER)mp2 )->pRecord )
                processEnter( pkscp, ( PKSCPRECORD )
                                     (( PNOTIFYRECORDENTER)mp2 )->pRecord );

            break;
    }

    return 0;
}

static MRESULT EXPENTRY windowProc( HWND hwnd, ULONG msg, MPARAM mp1,
                                    MPARAM mp2 )
{
    switch( msg )
    {
        case WM_CREATE  : return wmCreate( hwnd, mp1, mp2 );
        case WM_DESTROY : return wmDestroy( hwnd, mp1, mp2 );
        case WM_SIZE    : return wmSize( hwnd, mp1, mp2 );
        case WM_COMMAND : return wmCommand( hwnd, mp1,mp2 );
        case WM_PAINT   : return wmPaint( hwnd, mp1, mp2 );
        case WM_CONTROL : return wmControl( hwnd, mp1, mp2 );
    }

    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

int main( void )
{
    HAB   hab;
    HMQ   hmq;
    ULONG flFrameFlags;
    HWND  hwndFrame;
    HWND  hwndClient;
    QMSG  qm;
    int   rc;

    rc = libssh2_init( 0 );
    if( rc != 0 )
    {
        fprintf( stderr, "libssh2 initialization failed (%d)\n", rc );

        return 1;
    }

    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0 );

    WinRegisterClass( hab, WC_KSCP, windowProc, CS_SIZEREDRAW,
                      sizeof( PVOID ));

    flFrameFlags = FCF_SYSMENU | FCF_TITLEBAR | FCF_MINMAX | FCF_SIZEBORDER |
                   FCF_SHELLPOSITION | FCF_TASKLIST | FCF_MENU;

    hwndFrame = WinCreateStdWindow(
                    HWND_DESKTOP,               // parent window handle
                    WS_VISIBLE,                 // frame window style
                    &flFrameFlags,              // window style
                    WC_KSCP,                    // class name
                    KSCP_TITLE,                 // window title
                    0L,                         // default client style
                    NULLHANDLE,                 // resource in exe file
                    ID_KSCP,                    // frame window id
                    &hwndClient                 // client window handle
                );

    if( hwndFrame != NULLHANDLE )
    {
        BOOL  fShow = FALSE;
        ULONG ulBufMax;

        ulBufMax = sizeof( fShow );

        PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                             KSCP_PRF_KEY_SHOW, &fShow, &ulBufMax );

        if( fShow )
            WinPostMsg( hwndClient, WM_COMMAND,
                        MPFROMSHORT( IDM_FILE_ADDRBOOK ), 0 );

        while( WinGetMsg( hab, &qm, NULLHANDLE, 0, 0 ))
            WinDispatchMsg( hab, &qm );

        WinDestroyWindow( hwndFrame );
    }

    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    libssh2_exit();

    return 0;
}

