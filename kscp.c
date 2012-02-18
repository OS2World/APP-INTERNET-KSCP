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

#ifdef DEBUG
#define dprintf( ... ) \
do {\
    FILE *fp;\
    fp = fopen("kscp.log", "at");\
    fprintf( fp, __VA_ARGS__ );\
    fclose( fp );\
} while( 0 )
#else
#define dprintf( ... ) do { } while( 0 )
#endif

#define KSCP_PRF_APP               "KSCP"
#define KSCP_PRF_KEY_DLDIR         "DownloadDir"
#define KSCP_PRF_KEY_DLDIR_DEFAULT "."

#define KSCP_TITLE  "KSCP"

#define IDC_CONTAINER   1
#define ID_MSGBOX       99

typedef struct _KSCPDATA
{
    HWND                 hwnd;
    HWND                 hwndCnr;
    HWND                 hwndPopup;
    int                  sock;
    HMODULE              hmodPMWP;
    HPOINTER             hptrDefaultFile;
    HPOINTER             hptrDefaultFolder;
    HWND                 hwndDlg;
    LIBSSH2_SESSION     *session;
    LIBSSH2_SFTP        *sftp_session;
    char                *pszCurDir;
    char                *pszDlDir;
    BOOL                 fBusy;
    BOOL                 fCanceled;
} KSCPDATA, *PKSCPDATA;

static void removeRecordAll( PKSCPDATA pkscp )
{
    PRECORDCORE precc, preccNext;

    precc = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORD, NULL,
                        MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER ));
    for(; precc; precc = preccNext )
    {
        preccNext = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORD, precc,
                                MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ));

        free( precc->pszName );
        free( precc->pszText );

        WinSendMsg( pkscp->hwndCnr, CM_REMOVERECORD, &precc,
                    MPFROM2SHORT( 1, CMA_FREE ));
    }

    WinSendMsg( pkscp->hwndCnr, CM_INVALIDATERECORD, 0,
                MPFROM2SHORT( 0, CMA_REPOSITION ));
}

static void fileClose( PKSCPDATA pkscp )
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

static BOOL getServerInfo( PKSCPDATA pkscp, char **addr, char **username,
                           char **password, char **path )
{
    HWND  hwndDlg;
    ULONG ulReply = DID_CANCEL;

    hwndDlg = WinLoadDlg( HWND_DESKTOP, pkscp->hwnd, WinDefDlgProc,
                          NULLHANDLE, IDD_OPEN, NULL );
    if( hwndDlg )
    {

        ulReply = WinProcessDlg( hwndDlg );
        if( ulReply == DID_OK )
        {
            int len;

            len = WinQueryDlgItemTextLength( hwndDlg, IDCB_OPEN_ADDR ) + 1;
            *addr = malloc( len );
            WinQueryDlgItemText( hwndDlg, IDCB_OPEN_ADDR, len, *addr );

            len = WinQueryDlgItemTextLength( hwndDlg, IDEF_OPEN_USERNAME ) + 1;
            *username = malloc( len );
            WinQueryDlgItemText( hwndDlg, IDEF_OPEN_USERNAME, len, *username );

            len = WinQueryDlgItemTextLength( hwndDlg, IDEF_OPEN_PASSWORD ) + 1;
            *password = malloc( len );
            WinQueryDlgItemText( hwndDlg, IDEF_OPEN_PASSWORD, len, *password );

            *path = strdup("/");
        }

        WinDestroyWindow( hwndDlg );
    }

    return ( ulReply == DID_OK ) ? TRUE : FALSE;
}

static SHORT EXPENTRY fileCompare( PRECORDCORE p1, PRECORDCORE p2,
                                   PVOID pStorage )
{
    LIBSSH2_SFTP_ATTRIBUTES *pattr1, *pattr2;

    int rc;

    pattr1 = ( LIBSSH2_SFTP_ATTRIBUTES * )p1->pszText;
    pattr2 = ( LIBSSH2_SFTP_ATTRIBUTES * )p2->pszText;

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
    PRECORDCORE          precc;
    RECORDINSERT         ri;
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
            POINTERINFO pi;

            precc = WinSendMsg( pkscp->hwndCnr, CM_ALLOCRECORD, 0,
                                MPFROMLONG( 1 ));

            if(( attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS ) &&
               LIBSSH2_SFTP_S_ISDIR( attrs.permissions ))
                hptrIcon = pkscp->hptrDefaultFolder;

            WinQueryPointerInfo( hptrIcon, &pi );

            precc->cb            = sizeof( RECORDCORE );
            precc->hptrIcon      = pi.hbmMiniColor;
            precc->pszName       = strdup( mem );
            precc->pszText       = malloc( sizeof( attrs ));
            memcpy( precc->pszText, &attrs, sizeof( attrs ));

            ri.cb                = sizeof( RECORDINSERT );
            ri.pRecordOrder      = ( PRECORDCORE )CMA_END;
            ri.pRecordParent     = NULL;
            ri.zOrder            = ( USHORT )CMA_TOP;
            ri.fInvalidateRecord = FALSE;
            ri.cRecordsInsert    = 1;

            WinSendMsg( pkscp->hwndCnr, CM_INSERTRECORD, precc, &ri );
        }
    }

    libssh2_sftp_closedir( sftp_handle );

    WinSendMsg( pkscp->hwndCnr, CM_INVALIDATERECORD, 0,
                MPFROM2SHORT( 0, CMA_REPOSITION ));

    return TRUE;
}

static BOOL fileOpen( PKSCPDATA pkscp )
{
    struct sockaddr_in sin;
    const char        *fingerprint;
    char               szFingerprint[ 80 ];
    int                auth_pw = 1;
    char              *addr;
    char              *username;
    char              *password;
    char              *sftppath;
    struct hostent    *host;
    PFIELDINFO         pfi, pfiStart;
    FIELDINFOINSERT    fii;
    CNRINFO            ci;
    int                i;
    int                rc;
    RECTL              rcl;

    if( !getServerInfo( pkscp, &addr, &username, &password, &sftppath ))
        return FALSE;

    if( pkscp->sock != -1 )
        fileClose( pkscp );

    pkscp->pszCurDir = strdup( sftppath );

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    pkscp->sock = socket( AF_INET, SOCK_STREAM, 0 );

    host = gethostbyname( addr );

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
        if( libssh2_userauth_password( pkscp->session, username, password ))
        {
            printf("Authentication by password failed.\n");

            goto exit_session_disconnect;
        }
    }
    else
    {
        /* Or by public key */
        if( libssh2_userauth_publickey_fromfile( pkscp->session, username,
                            "/home/username/.ssh/id_rsa.pub",
                            "/home/username/.ssh/id_rsa",
                            password ))
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
                                      CCS_EXTENDSEL, 0, 0, 0, 0,
                                      pkscp->hwnd, HWND_TOP, IDC_CONTAINER,
                                      NULL, NULL );

    pfi = pfiStart = WinSendMsg( pkscp->hwndCnr, CM_ALLOCDETAILFIELDINFO,
                                 MPFROMLONG( 2 ), NULL );

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_BITMAPORICON | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR;
    pfi->flTitle    = CFA_CENTER;
    pfi->pTitleData = "Icon";
    pfi->offStruct  = FIELDOFFSET(RECORDCORE, hptrIcon );
    pfi             = pfi->pNextFieldInfo;

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR;
    pfi->flTitle    = CFA_CENTER;
    pfi->pTitleData = "Name";
    pfi->offStruct  = FIELDOFFSET(RECORDCORE, pszName);

    fii.cb                   = ( ULONG )( sizeof( FIELDINFOINSERT ));
    fii.pFieldInfoOrder      = (PFIELDINFO)CMA_FIRST;
    fii.cFieldInfoInsert     = 2;
    fii.fInvalidateFieldInfo = TRUE;

    WinSendMsg( pkscp->hwndCnr, CM_INSERTDETAILFIELDINFO,
                MPFROMP( pfiStart ), MPFROMP( &fii ));

    ci.pSortRecord  = fileCompare;
    ci.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES | CA_DRAWBITMAP;
    ci.slBitmapOrIcon.cx = WinQuerySysValue( HWND_DESKTOP, SV_CYMENU );
    ci.slBitmapOrIcon.cy = ci.slBitmapOrIcon.cx;

    WinSendMsg( pkscp->hwndCnr, CM_SETCNRINFO, MPFROMP( &ci ),
                MPFROMLONG( CMA_PSORTRECORD | CMA_FLWINDOWATTR |
                            CMA_SLBITMAPORICON ));

    if( !readDir( pkscp, sftppath ))
        goto exit_destroy_window;


    WinQueryWindowRect( pkscp->hwnd, &rcl );
    WinSetWindowPos( pkscp->hwndCnr, HWND_TOP,
                     rcl.xLeft, rcl.yBottom,
                     rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                     SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_SHOW );

    free( addr );
    free( username );
    free( password );
    free( sftppath );

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

    free( addr );
    free( username );
    free( password );
    free( sftppath );

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
                           KSCP_PRF_KEY_DLDIR_DEFAULT, szStr, sizeof( szStr ));

    pkscp->pszDlDir = strdup( szStr );

    return 0;
}

static MRESULT wmDestroy( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp = WinQueryWindowPtr( hwnd, 0 );

    fileClose( pkscp );

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

static void fileDlDir( PKSCPDATA pkscp )
{
    HWND hwndDlg;

    hwndDlg = WinLoadDlg( HWND_DESKTOP, pkscp->hwnd, WinDefDlgProc,
                          0, IDD_DLDIR, NULL );

    if( hwndDlg )
    {
        ULONG ulReply;

        WinSendDlgItemMsg( hwndDlg, IDT_DLDIR_DIRNAME, EM_SETTEXTLIMIT,
                           MPFROMSHORT( 1024 ), 0 );

        WinSetDlgItemText( hwndDlg, IDT_DLDIR_DIRNAME, pkscp->pszDlDir );

        // select the entire text
        WinSendDlgItemMsg( hwndDlg, IDT_DLDIR_DIRNAME, EM_SETSEL,
                           MPFROM2SHORT( 0, 1024 ), 0 );

        ulReply = WinProcessDlg( hwndDlg );
        if( ulReply == DID_OK )
        {
            int len;

            free( pkscp->pszDlDir );

            len = WinQueryDlgItemTextLength(hwndDlg, IDT_DLDIR_DIRNAME ) + 1;
            pkscp->pszDlDir = malloc( len );
            WinQueryDlgItemText( hwndDlg, IDT_DLDIR_DIRNAME, len,
                                 pkscp->pszDlDir );
        }

        WinDestroyWindow( hwndDlg );
    }
}

#define BUF_SIZE    ( 1024 * 4 )

static int download( PKSCPDATA pkscp, PRECORDCORE precc )
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
              pkscp->pszCurDir, precc->pszName );
    sftp_handle = libssh2_sftp_open( pkscp->sftp_session, sftppath,
                                     LIBSSH2_FXF_READ, 0 );
    if( !sftp_handle )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot open %s", sftppath );

        WinMessageBox( HWND_DESKTOP, pkscp->hwnd, szMsg, "Open", ID_MSGBOX,
                       MB_OK | MB_ERROR );

        return 1;
    }

    pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )precc->pszText;

    buf = malloc( BUF_SIZE );
    _makepath( buf, NULL, pkscp->pszDlDir, precc->pszName, NULL );

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
    PRECORDCORE precc;
    int         count;

    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    precc = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                        MPFROMLONG( CMA_FIRST ),
                        MPFROMLONG( CRA_SELECTED ));

    for( count = 0; precc; )
    {
        pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )precc->pszText;
        if( LIBSSH2_SFTP_S_ISREG( pattr->permissions ))
            count++;

        precc = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                            MPFROMP( precc ),
                            MPFROMLONG( CRA_SELECTED ));
    }

    return count;
}

static void downloadThread( void *arg )
{
    PKSCPDATA   pkscp = ( PKSCPDATA )arg;

    HAB hab;
    HMQ hmq;

    PRECORDCORE precc;

    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    char szMsg[ 100 ];
    int  count;
    int  i;

    pkscp->fBusy = TRUE;

    // to use WinSendMsg()
    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0);

    count = checkDlFiles( pkscp );

    precc = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                        MPFROMLONG( CMA_FIRST ),
                        MPFROMLONG( CRA_SELECTED ));

    for( i = 0; precc && !pkscp->fCanceled;  )
    {
        pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )precc->pszText;
        if( LIBSSH2_SFTP_S_ISREG( pattr->permissions ))
        {
            i++;
            sprintf( szMsg, "%d of %d", i, count );
            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_INDEX,
                               szMsg );
            WinSetDlgItemText( pkscp->hwndDlg, IDT_DOWNLOAD_FILENAME,
                               precc->pszName );

            download( pkscp, precc );
        }

        precc = WinSendMsg( pkscp->hwndCnr, CM_QUERYRECORDEMPHASIS,
                            MPFROMP( precc ),
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

    WinSetWindowText( pkscp->hwndDlg, "Upload");

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

static MRESULT wmCommand( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKSCPDATA pkscp = WinQueryWindowPtr( hwnd, 0 );

    switch( SHORT1FROMMP( mp1 ))
    {
        case IDM_FILE_OPEN  :
            fileOpen( pkscp );
            break;

        case IDM_FILE_CLOSE :
            fileClose( pkscp );
            break;

        case IDM_FILE_DLDIR :
            fileDlDir( pkscp );
            break;

        case IDM_FILE_EXIT  :
            WinPostMsg( hwnd, WM_QUIT, 0, 0 );

        case IDM_KSCP_DOWNLOAD :
            kscpDownload( pkscp );
            break;

        case IDM_KSCP_UPLOAD :
            kscpUpload( pkscp );
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

static void processEnter( PKSCPDATA pkscp, PRECORDCORE precc )
{
    LIBSSH2_SFTP_ATTRIBUTES *pattr;

    pattr = ( LIBSSH2_SFTP_ATTRIBUTES * )precc->pszText;
    if( LIBSSH2_SFTP_S_ISDIR( pattr->permissions ))
    {
        char *pszNewDir = NULL;

        if( strcmp( precc->pszName, ".."))
            asprintf( &pszNewDir, "%s%s/", pkscp->pszCurDir, precc->pszName );
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
                processEnter( pkscp, (( PNOTIFYRECORDENTER)mp2 )->pRecord );

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
        while( WinGetMsg( hab, &qm, NULLHANDLE, 0, 0 ))
            WinDispatchMsg( hab, &qm );

        WinDestroyWindow( hwndFrame );
    }

    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    libssh2_exit();

    return 0;
}

