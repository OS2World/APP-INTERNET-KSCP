#define INCL_WIN
#define INCL_DOS
#include <os2.h>

#include <ctime>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <KDirDlg.h>

#include "kscp.h"

#include "KRemoteWorkThread.h"
#include "KLocalWorkThread.h"

#include "KSCPClient.h"

#define IDC_CONTAINER   1

#define KSCP_PRF_KEY_DLDIR  "DownloadDir"

bool KSCPClient::ReadDir( const char* dir )
{
    LIBSSH2_SFTP_HANDLE* sftp_handle;
    PKSCPRECORD          pkr;
    RECORDINSERT         ri;
    ULONG                ulStyle;
    int                  rc;

    fprintf( stderr, "libssh2_sftp_opendir()!\n");
    /* Request a dir listing via SFTP */
    sftp_handle = libssh2_sftp_opendir( _sftp_session, dir );

    if (!sftp_handle)
    {
        MessageBox("Unable to open dir with SFTP", "OpenDir",
                   MB_OK | MB_ERROR );

        return false;
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
            HPOINTER hptrIcon = _hptrDefaultFile;

            pkr = _kcnr.AllocRecord( 1 );

            if(( attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS ) &&
               LIBSSH2_SFTP_S_ISDIR( attrs.permissions ))
                hptrIcon = _hptrDefaultFolder;

            pkr->pszName = new char[ strlen( mem ) + 1 ];
            strcpy( pkr->pszName, mem );

            pkr->pbAttr = new BYTE[ sizeof( attrs )];
            memcpy( pkr->pbAttr, &attrs, sizeof( attrs ));

            if( hptrIcon != _hptrDefaultFolder &&
                attrs.flags & LIBSSH2_SFTP_ATTR_SIZE )
            {
                static const char* pszUnit[] = {"B", "KB", "MB", "GB", "TB"};

                unsigned usUnit = 0;
                double   dSize = attrs.filesize;

                for( usUnit = 0;
                     usUnit < sizeof( pszUnit ) / sizeof( pszUnit[ 0 ]) - 1; )
                {
                    if( dSize > 1024.0 )
                    {
                        dSize /= 1024.0;
                        usUnit++;
                    }
                    else
                        break;
                }

                snprintf( mem, sizeof( mem ),"%.*f %s",
                          usUnit == 0 ? 0 : 2, dSize, pszUnit[ usUnit ]);

                pkr->pszSize = strdup( mem );
            }
            else
                pkr->pszSize = 0;

            if( strcmp( pkr->pszName, "..") &&
                attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME )
            {
               struct tm tm = *localtime( reinterpret_cast< time_t* >
                                            ( &attrs.mtime ));

               snprintf( mem, sizeof( mem ),
                         "%04d-%02d-%02d, %02d:%02d",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min );

                pkr->pszDate = strdup( mem );
            }
            else
                pkr->pszDate = 0;

            //pkr->mrc.cb        = sizeof( KSCPRECORD );
            pkr->mrc.hptrIcon  = hptrIcon;
            pkr->mrc.pszIcon   = pkr->pszName;

            ri.cb                = sizeof( RECORDINSERT );
            ri.pRecordOrder      = reinterpret_cast< PRECORDCORE >( CMA_END );
            ri.pRecordParent     = NULL;
            ri.zOrder            = CMA_TOP;
            ri.fInvalidateRecord = false;
            ri.cRecordsInsert    = 1;

            _kcnr.InsertRecord( pkr, &ri );
        }
    }

    libssh2_sftp_closedir( sftp_handle );

    _kcnr.InvalidateRecord( 0, 0, CMA_REPOSITION );

    // Change a selection type to a single selection
    ulStyle  = _kcnr.QueryWindowULong( QWL_STYLE );
    ulStyle &= ~CCS_EXTENDSEL;
    ulStyle |=  CCS_SINGLESEL;
    _kcnr.SetWindowULong( QWL_STYLE, ulStyle );

    // Select the first item and clear emphasis of other item
    _kcnr.SetRecordEmphasis(
        _kcnr.QueryRecord( 0, CMA_FIRST, CMA_ITEMORDER ), true,
        CRA_CURSORED | CRA_SELECTED );

    // Change a selection type to a extend selection
    ulStyle  = _kcnr.QueryWindowULong( QWL_STYLE );
    ulStyle &= ~CCS_SINGLESEL;
    ulStyle |=  CCS_EXTENDSEL;
    _kcnr.SetWindowULong( QWL_STYLE, ulStyle );

    // Scroll to the top
    _kcnr.PostMsg( WM_CHAR, MPFROMSHORT( KC_VIRTUALKEY ),
                   MPFROM2SHORT( 0, VK_HOME ));

    return true;
}

bool KSCPClient::KSCPConnect( PSERVERINFO psi )
{
    struct sockaddr_in sin;
    const char*        fingerprint;
    char               szMsg[ 512 ];
    int                auth_pw = 1;
    struct hostent*    host;
    PFIELDINFO         pfi, pfiStart;
    FIELDINFOINSERT    fii;
    CNRINFO            ci;
    int                i;
    int                rc;
    RECTL              rcl;

    KWindow kwnd;

    if( _sock != -1 )
        KSCPDisconnect();

    _strCurDir = psi->szDir;

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    _sock = socket( AF_INET, SOCK_STREAM, 0 );

    host = gethostbyname( psi->szAddress );
    if( !host )
    {
        snprintf( szMsg, sizeof( szMsg ),
                  "Cannot resolve host %s", psi->szAddress );

        MessageBox( szMsg, "Connect", MB_OK | MB_ERROR );

        goto exit_close_socket;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons( 22 );
    sin.sin_addr.s_addr = *reinterpret_cast< u_long* >( host->h_addr );
    if( connect( _sock, reinterpret_cast< struct sockaddr* >( &sin ),
                 sizeof( struct sockaddr_in )) != 0 )
    {
        fprintf(stderr, "failed to connect!\n");

        goto exit_close_socket;
    }

    /* Create a session instance
     */
    _session = libssh2_session_init();
    if( !_session )
        goto exit_close_socket;

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake( _session, _sock );
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
    fingerprint = libssh2_hostkey_hash( _session,
                                        LIBSSH2_HOSTKEY_HASH_SHA1 );
    for( i = 0; i < 20; i++ )
        sprintf( szMsg + i * 3 , "%02X ", ( unsigned char )fingerprint[ i ]);

    MessageBox( szMsg, "Fingerprint", MB_OK | MB_INFORMATION );

    if( auth_pw )
    {
        /* We could authenticate via password */
        if( libssh2_userauth_password( _session, psi->szUserName,
                                       psi->szPassword ))
        {
            printf("Authentication by password failed.\n");

            goto exit_session_disconnect;
        }
    }
    else
    {
        /* Or by public key */
        if( libssh2_userauth_publickey_fromfile( _session,
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
    _sftp_session = libssh2_sftp_init( _session );

    if( !_sftp_session )
    {
        fprintf( stderr, "Unable to init SFTP session\n");

        goto exit_session_disconnect;
    }

    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking( _session, 1 );

    _kcnr.CreateWindow( this, NULL,
                       CCS_AUTOPOSITION | CCS_EXTENDSEL |
                       CCS_MINIRECORDCORE | CCS_MINIICONS,
                       0, 0, 0, 0, this, KWND_TOP, IDC_CONTAINER );

    pfi = pfiStart = _kcnr.AllocDetailFieldInfo( 4 );

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_BITMAPORICON | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR;
    pfi->flTitle    = CFA_CENTER | CFA_FITITLEREADONLY;
    pfi->pTitleData = const_cast< char* >("Icon");
    pfi->offStruct  = FIELDOFFSET(KSCPRECORD, mrc.hptrIcon );
    pfi             = pfi->pNextFieldInfo;

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR;
    pfi->flTitle    = CFA_CENTER | CFA_FITITLEREADONLY;
    pfi->pTitleData = const_cast< char* >("Name");
    pfi->offStruct  = FIELDOFFSET(KSCPRECORD, pszName);
    pfi             = pfi->pNextFieldInfo;

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_STRING | CFA_HORZSEPARATOR | CFA_RIGHT |
                      CFA_SEPARATOR | CFA_FIREADONLY;
    pfi->flTitle    = CFA_CENTER | CFA_FITITLEREADONLY;
    pfi->pTitleData = const_cast< char* >("Size");
    pfi->offStruct  = FIELDOFFSET(KSCPRECORD, pszSize);
    pfi             = pfi->pNextFieldInfo;

    pfi->cb         = sizeof( FIELDINFO );
    pfi->flData     = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER |
                      CFA_SEPARATOR | CFA_FIREADONLY;
    pfi->flTitle    = CFA_CENTER | CFA_FITITLEREADONLY;
    pfi->pTitleData = const_cast< char* >("Date");
    pfi->offStruct  = FIELDOFFSET(KSCPRECORD, pszDate);

    fii.cb                   = sizeof( FIELDINFOINSERT );
    fii.pFieldInfoOrder      = reinterpret_cast< PFIELDINFO >( CMA_FIRST );
    fii.cFieldInfoInsert     = 4;
    fii.fInvalidateFieldInfo = true;

    _kcnr.InsertDetailFieldInfo( pfiStart, &fii );

    ci.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES | CA_DRAWICON |
                      CV_MINI;
    ci.slBitmapOrIcon.cx = 0;
    ci.slBitmapOrIcon.cy = ci.slBitmapOrIcon.cx;

    _kcnr.SetCnrInfo( &ci,
                      CMA_PSORTRECORD | CMA_FLWINDOWATTR |
                      CMA_SLBITMAPORICON );

    if( !ReadDir( psi->szDir ))
        goto exit_destroy_window;

    QueryWindowRect( &rcl );
    _kcnr.SetWindowPos( KWND_TOP, rcl.xLeft, rcl.yBottom,
                       rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                       SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_SHOW );
    _kcnr.SetFocus();

    return true;

exit_destroy_window :
    _kcnr.DestroyWindow();

    libssh2_sftp_shutdown( _sftp_session );
    _sftp_session = NULL;

exit_session_disconnect :
    libssh2_session_disconnect( _session, "Abnormal Shutdown");

exit_session_free :
    libssh2_session_free( _session );
    _session = NULL;

exit_close_socket :
    close( _sock );
    _sock = -1;

    _strCurDir.clear();

    return false;
}

void KSCPClient::RemoveRecordAll()
{
    PKSCPRECORD pkr, pkrNext;

    pkr = _kcnr.QueryRecord( 0, CMA_FIRST, CMA_ITEMORDER );
    for(; pkr; pkr = pkrNext )
    {
        pkrNext = _kcnr.QueryRecord( pkr, CMA_NEXT, CMA_ITEMORDER );

        delete[] pkr->pszName;
        delete[] pkr->pbAttr;
        free( pkr->pszSize );
        free( pkr->pszDate );

        _kcnr.RemoveRecord( &pkr, 1, CMA_FREE );
    }

    _kcnr.InvalidateRecord( 0, 0, CMA_REPOSITION );
}

void KSCPClient::KSCPDisconnect()
{
    if( _sock == -1 )
        return;

    RemoveRecordAll();

    _kcnr.RemoveDetailFieldInfo( 0, 0, CMA_FREE | CMA_INVALIDATE );

    _kcnr.DestroyWindow();

    libssh2_sftp_shutdown( _sftp_session );
    _sftp_session = NULL;

    libssh2_session_disconnect( _session,
                                "Normal Shutdown, Thank you for playing");
    libssh2_session_free( _session );
    _session = NULL;

    close( _sock );
    _sock = -1;

    _strCurDir.clear();
}

bool KSCPClient::FileOpen()
{
    PSERVERINFO psi = new SERVERINFO();
    ULONG       rc = false;

    if( getServerInfo( this, psi, false ))
        rc = KSCPConnect( psi );

    delete psi;

    return rc;
}

void KSCPClient::FileClose()
{
    KSCPDisconnect();
}

bool KSCPClient::FileAddrBook()
{
    PSERVERINFO psi = new SERVERINFO();
    ULONG       rc = false;

    if( abDlg( this, psi ))
        rc = KSCPConnect( psi );

    delete psi;

    return rc;
}

void KSCPClient::FileDlDir()
{
    KDirDlg kdd;

    kdd.Clear();
    kdd.SetTitle("Choose a download directory");
    kdd.SetFullFile( _strDlDir.c_str());
    if( kdd.FileDlg( KWND_DESKTOP, this ) && kdd.GetReturn() == DID_OK )
        _strDlDir = kdd.GetFullFile();
}

PKSCPRECORD KSCPClient::FindRecord( PKSCPRECORD pkrStart, ULONG ulEM,
                                    bool fWithDir )
{
    PKSCPRECORD pkr;

    LIBSSH2_SFTP_ATTRIBUTES* pattr;

    pkr = _kcnr.QueryRecordEmphasis( pkrStart ? pkrStart :
                                                reinterpret_cast< PKSCPRECORD >
                                                    ( CMA_FIRST ),
                                     ulEM );

    while( pkr )
    {
        pattr = reinterpret_cast< LIBSSH2_SFTP_ATTRIBUTES* >( pkr->pbAttr );
        if( LIBSSH2_SFTP_S_ISREG( pattr->permissions ))
            break;
        else if( fWithDir && LIBSSH2_SFTP_S_ISDIR( pattr->permissions ) &&
                 strcmp(  pkr->pszName, ".."))
            break;

        pkr = _kcnr.QueryRecordEmphasis( pkr, ulEM );
    }

    return pkr;
}

int KSCPClient::CountRecords( ULONG ulEM, bool fWithDir )
{
    PKSCPRECORD pkr;
    int         count;

    for( pkr = NULL, count = 0;;)
    {
        pkr = FindRecord( pkr, ulEM, fWithDir );
        if( !pkr )
            break;

        count++;
    }

    return count;
}

void KSCPClient::Refresh()
{
    RemoveRecordAll();

    ReadDir( _strCurDir.c_str());
}

#define BUF_SIZE    ( 1024 * 4 )

int KSCPClient::Download( PKSCPRECORD pkr )
{
    LIBSSH2_SFTP_HANDLE*     sftp_handle;
    char                     sftppath[ 512 ];
    LIBSSH2_SFTP_ATTRIBUTES* pattr;

    struct stat       statbuf;
    FILE*             fp;
    char*             buf;
    libssh2_uint64_t  size;
    char              szMsg[ 512 ];

    struct timeval tv1, tv2;
    long long      diffTime;

    int rc = 1;

    snprintf( sftppath, sizeof( sftppath ), "%s%s",
              _strCurDir.c_str(), pkr->pszName );
    sftp_handle = libssh2_sftp_open( _sftp_session, sftppath,
                                     LIBSSH2_FXF_READ, 0 );
    if( !sftp_handle )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot open %s", sftppath );

        MessageBox( szMsg, "Download", MB_OK | MB_ERROR );

        return rc;
    }

    pattr = reinterpret_cast< LIBSSH2_SFTP_ATTRIBUTES* >( pkr->pbAttr );

    buf = new char[ BUF_SIZE ];
    _makepath( buf, NULL, _strDlDir.c_str(), pkr->pszName, NULL );

    if( !stat( buf, &statbuf ))
    {
        ULONG ulReply;

        snprintf( szMsg, sizeof( szMsg ),
                  "%s\nalready exists. Overwrite ?", buf );

        ulReply = MessageBox( szMsg, "Download", MB_YESNO | MB_ICONQUESTION );

        if( ulReply == MBID_NO )
            goto exit_free;
    }

    fp = fopen( buf, "wb");
    if( !fp )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot create %s", buf );

        MessageBox( szMsg ,"Download", MB_OK | MB_ERROR );

        goto exit_free;
    }

    if( pattr->filesize )
    {
        int nRead;

        for( size = diffTime = 0; !_fCanceled; )
        {
            sprintf( szMsg, "%lld KB of %lld KB (%lld%%)",
                     size / 1024, pattr->filesize / 1024,
                     size * 100 / pattr->filesize );

            _kdlg.SetDlgItemText( IDT_DOWNLOAD_STATUS, szMsg );

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
                _kdlg.SetDlgItemText( IDT_DOWNLOAD_SPEED, szMsg );
            }
        }
    }

    fclose( fp );

    rc = 0;

exit_free:
    delete[] buf;

    libssh2_sftp_close( sftp_handle );

    return rc;
}

void KSCPClient::RemoteMain( PFN_REMOTE_CALLBACK pCallback )
{
    PKSCPRECORD pkr;

    char szMsg[ 512 ];
    int  count;
    int  i;

    _fBusy = true;

    count = CountRecords( CRA_SELECTED, false );

    for( i = 1, pkr = NULL; !_fCanceled; i++ )
    {
        pkr = FindRecord( pkr, CRA_SELECTED, false );
        if( !pkr )
            break;

        sprintf( szMsg, "%d of %d", i, count );
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_INDEX,  szMsg );
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_FILENAME, pkr->pszName );

        ( this->*pCallback )( pkr );
    }

    _kdlg.DismissDlg( _fCanceled ? DID_CANCEL : DID_OK );

    _fBusy = false;
}

int KSCPClient::KSCPDownload()
{
    char  szMsg[ 100 ];
    ULONG ulReply;

    if( _fBusy )
    {
        MessageBox("Session is busy\nTry again, later", "Download",
                   MB_OK | MB_ERROR );

        return 1;
    }

    if( !CountRecords( CRA_SELECTED, false ))
    {
        MessageBox("Files not selected", "Download", MB_OK | MB_ERROR );

        return 1;
    }

    _fCanceled = false;

    _kdlg.LoadDlg( KWND_DESKTOP, this, 0, IDD_DOWNLOAD );

    RemoteParam rpParam = { this, &KSCPClient::Download };
    KRemoteWorkThread thread;
    thread.BeginThread( &rpParam );

    _kdlg.ProcessDlg();
    ulReply = _kdlg.GetResult();
    if( ulReply == DID_CANCEL )
        _fCanceled = true;

    _kdlg.DestroyWindow();

    sprintf( szMsg, "Download %s",
             ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    MessageBox( szMsg, "Download", MB_OK | MB_INFORMATION );

    return 0;
}

int KSCPClient::Upload( const char* pszName )
{
    LIBSSH2_SFTP_HANDLE*    sftp_handle;
    char                    sftppath[ 512 ];
    LIBSSH2_SFTP_ATTRIBUTES sftp_attrs;

    FILE* fp;
    off_t size, fileSize;
    char* buf;
    char  szMsg[ 512 ];

    struct stat statbuf;

    struct timeval tv1, tv2;
    long long      diffTime;

    int rc = 1;

    if( stat( pszName, &statbuf ) < 0 || !S_ISREG( statbuf.st_mode ))
    {
        snprintf( szMsg, sizeof( szMsg ),
                 "Ooops... This is not a file. Ignored.\n%s", pszName );

        MessageBox( szMsg , "Upload", MB_OK | MB_ERROR );

        return rc;
    }

    fp = fopen( pszName, "rb");
    if( !fp )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot open %s", pszName );

        MessageBox( szMsg , "Upload", MB_OK | MB_ERROR );

        return rc;
    }

    fseeko( fp, 0, SEEK_END );
    fileSize = ftello( fp );
    fseeko( fp, 0, SEEK_SET );

    snprintf( sftppath, sizeof( sftppath ), "%s%s",
              _strCurDir.c_str(), strrchr( pszName, '\\') + 1 );

    if( !libssh2_sftp_stat( _sftp_session, sftppath, &sftp_attrs ))
    {
        ULONG ulReply;

        snprintf( szMsg, sizeof( szMsg ),
                  "%s\nalready exists. Overwrite ?", sftppath );

        ulReply = MessageBox( szMsg , "Upload", MB_YESNO | MB_ICONQUESTION );

        if( ulReply == MBID_NO )
            goto exit_fclose;
    }

    sftp_handle = libssh2_sftp_open( _sftp_session, sftppath,
                                     LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT |
                                     LIBSSH2_FXF_TRUNC,
                                     LIBSSH2_SFTP_S_IRUSR |
                                     LIBSSH2_SFTP_S_IWUSR |
                                     LIBSSH2_SFTP_S_IRGRP |
                                     LIBSSH2_SFTP_S_IROTH );
    if( !sftp_handle )
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot create %s", sftppath );

        MessageBox( szMsg , "Upload", MB_OK | MB_ERROR );

        goto exit_fclose;
    }

    buf = new char[ BUF_SIZE ];

    if( fileSize )
    {
        int         nRead, nWrite;
        const char* ptr;

        for( size = diffTime = 0; !_fCanceled; )
        {
            sprintf( szMsg, "%lld KB of %lld KB (%lld%%)",
                     size / 1024, fileSize / 1024,
                     size * 100 / fileSize );

            _kdlg.SetDlgItemText( IDT_DOWNLOAD_STATUS, szMsg );

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
                MessageBox( szMsg , "Upload",
                                          MB_OK | MB_ERROR );
                goto exit_free;
            }

            if( diffTime )
            {
                sprintf( szMsg, "%lld KB/s",
                         ( size * 1000000LL / 1024 ) / diffTime );
                _kdlg.SetDlgItemText( IDT_DOWNLOAD_SPEED, szMsg );
            }
        }
    }

    libssh2_sftp_close( sftp_handle );

    rc = 0;

exit_free :
    delete[] buf;

exit_fclose :
    fclose( fp );

    return rc;
}

void KSCPClient::LocalMain( PFN_LOCAL_CALLBACK pCallback )
{
    APSZ  apszName = { const_cast< PSZ >( _kfd.GetFullFile())};
    PAPSZ papszList = _kfd.GetFQFilename();
    ULONG ulCount   = _kfd.GetFQFCount();

    char     szMsg[ 100 ];
    unsigned i;

    _fBusy = true;

    if( !papszList )
        papszList = &apszName;

    for( i = 0; i < ulCount && !_fCanceled; i++ )
    {
        sprintf( szMsg, "%d of %ld", i + 1, ulCount );
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_INDEX,  szMsg );
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_FILENAME, (*papszList)[ i ]);
        (this->*pCallback)((*papszList)[ i ]);
    }

    _kdlg.DismissDlg( _fCanceled ? DID_CANCEL : DID_OK );

    _fBusy = false;
}

int KSCPClient::KSCPUpload()
{
    char       szMsg[ 100 ];
    ULONG      ulReply;

    if( _fBusy )
    {
        MessageBox("Session is busy\nTry again, later", "Upload",
                   MB_OK | MB_ERROR );

        return 1;
    }

    _kfd.Clear();
    _kfd.SetFl( FDS_CENTER | FDS_MULTIPLESEL | FDS_OPEN_DIALOG );
    _kfd.SetTitle("Upload");
    if( !_kfd.FileDlg( KWND_DESKTOP, this ) || _kfd.GetReturn() != DID_OK )
        return 1;

    _fCanceled = false;

    _kdlg.LoadDlg( KWND_DESKTOP, this, 0, IDD_DOWNLOAD );

    _kdlg.SetWindowText("Upload");

    LocalParam lpParam = { this, &KSCPClient::Upload };
    KLocalWorkThread thread;
    thread.BeginThread( &lpParam );

    _kdlg.ProcessDlg();
    ulReply = _kdlg.GetResult();
    if( ulReply == DID_CANCEL )
        _fCanceled = true;

    _kdlg.DestroyWindow();

    sprintf( szMsg, "Upload %s",
             ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    MessageBox( szMsg, "Upload", MB_OK | MB_INFORMATION );

    Refresh();

    return 0;
}

int KSCPClient::Delete( PKSCPRECORD pkr )
{
    char sftppath[ 512 ];
    char szMsg[ 512 ];

    snprintf( sftppath, sizeof( sftppath ), "%s%s",
              _strCurDir.c_str(), pkr->pszName );

    if( libssh2_sftp_unlink( _sftp_session, sftppath ))
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot delete %s", sftppath );

        MessageBox( szMsg, "Delete", MB_OK | MB_ERROR );

        return 1;
    }

    return 0;
}

int KSCPClient::KSCPDelete()
{
    char  szMsg[ 100 ];
    ULONG ulReply;

    if( _fBusy )
    {
        MessageBox("Session is busy\nTry again, later", "Delete",
                   MB_OK | MB_ERROR );

        return 1;
    }

    if( !CountRecords( CRA_SELECTED, false ))
    {
        MessageBox("Files not selected", "Delete", MB_OK | MB_ERROR );

        return 1;
    }

    if( MessageBox("Are you sure to delete ?", "Delete",
                   MB_YESNO | MB_ICONQUESTION ) == MBID_NO )
        return 1;

    _fCanceled = false;

    _kdlg.LoadDlg( KWND_DESKTOP, this, 0, IDD_DOWNLOAD );

    _kdlg.SetWindowText("Delete");
    _kdlg.SetDlgItemText( IDT_DOWNLOAD_STATUS, "");
    _kdlg.SetDlgItemText( IDT_DOWNLOAD_SPEED, "");

    RemoteParam rpParam = { this, &KSCPClient::Delete };
    KRemoteWorkThread thread;
    thread.BeginThread( &rpParam );

    _kdlg.ProcessDlg();
    ulReply = _kdlg.GetResult();
    if( ulReply == DID_CANCEL )
        _fCanceled = true;

    _kdlg.DestroyWindow();

    sprintf( szMsg, "Delete %s",
             ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    MessageBox( szMsg, "Delete", MB_OK | MB_INFORMATION );

    Refresh();

    return 0;
}

void KSCPClient::Rename( PKSCPRECORD pkr )
{
    char oldsftppath[ 512 ], newsftppath[ 512 ];
    char szMsg[ 512 ];

    snprintf( oldsftppath, sizeof( oldsftppath ), "%s%s",
              _strCurDir.c_str(), pkr->mrc.pszIcon );

    snprintf( newsftppath, sizeof( newsftppath ), "%s%s",
              _strCurDir.c_str(), pkr->pszName );

    if( strcmp( oldsftppath, newsftppath) &&
        libssh2_sftp_rename( _sftp_session, oldsftppath, newsftppath ))
    {
        snprintf( szMsg, sizeof( szMsg ), "Cannot rename %s to %s",
                  pkr->mrc.pszIcon, pkr->pszName );

        MessageBox( szMsg , "Rename", MB_OK | MB_ERROR );

        delete[] pkr->pszName;
        pkr->pszName = pkr->mrc.pszIcon;
    }
    else
    {
        // Now free mrc.pszIcon to prevent memroy leak
        // See the comment at CN_REALLOCPSZ
        delete[] pkr->mrc.pszIcon;

        // Sync with pszName
        pkr->mrc.pszIcon = pkr->pszName;
    }

    _kcnr.SortRecordP();
}

int KSCPClient::KSCPRename()
{
    PKSCPRECORD pkr;
    PFIELDINFO  pfi;
    CNREDITDATA ced;

    int  count;

    if( _fBusy )
    {
        MessageBox("Session is busy\nTry again, later", "Rename",
                   MB_OK | MB_ERROR );

        return 1;
    }

    count = CountRecords( CRA_SELECTED, true );

    if( count == 0 )
    {
        MessageBox("Files not selected", "Rename", MB_OK | MB_ERROR );

        return 1;
    }

    pkr = FindRecord( NULL, CRA_SELECTED, true );

    pfi = _kcnr.QueryDetailFieldInfo( 0, CMA_FIRST );

    while( pfi && pfi->offStruct != FIELDOFFSET( KSCPRECORD, pszName ))
        pfi = _kcnr.QueryDetailFieldInfo( pfi, CMA_NEXT );

    memset( &ced, 0, sizeof( ced ));
    ced.cb         = sizeof( ced );
    ced.pRecord    = ( PRECORDCORE )pkr;
    ced.pFieldInfo = pfi;
    ced.id         = CID_LEFTDVWND;

    _kcnr.OpenEdit( &ced );

    return 0;
}

MRESULT KSCPClient::CnContextMenu()
{
    POINTL ptl;
    ULONG  fs = PU_NONE | PU_KEYBOARD | PU_MOUSEBUTTON1 |
                PU_HCONSTRAIN | PU_VCONSTRAIN;

    QueryPointerPos( &ptl );
    MapWindowPoints( KWND_DESKTOP, this, &ptl, 1 );

    _kmenuPopup.PopupMenu( this, this, ptl.x, ptl.y, 0, fs );

    return 0;
}

MRESULT KSCPClient::CnEnter( ULONG ulParam )
{
    PNOTIFYRECORDENTER pnre = reinterpret_cast< PNOTIFYRECORDENTER >
                                    ( ulParam );
    PKSCPRECORD pkr = reinterpret_cast< PKSCPRECORD >( pnre->pRecord );

    // pRecord is NULL if a heading of container is double-clicked
    if( !pkr )
        return 0;

    LIBSSH2_SFTP_ATTRIBUTES* pattr;

    pattr = reinterpret_cast< LIBSSH2_SFTP_ATTRIBUTES* >( pkr->pbAttr );
    if( LIBSSH2_SFTP_S_ISDIR( pattr->permissions ))
    {
        char* pszNewDir = NULL;

        if( strcmp( pkr->pszName, ".."))
            asprintf( &pszNewDir, "%s%s/", _strCurDir.c_str(), pkr->pszName );
        else if( _strCurDir[ 1 ]) // not root ?
        {
            // remove the last '/'
           _strCurDir[ _strCurDir.length() - 1 ] = '\0';

            // remove the last directory part
            _strCurDir[ _strCurDir.find_last_of('/') + 1 ] = '\0';

            pszNewDir = strdup( _strCurDir.c_str() );
        }

        if( pszNewDir )
        {
            RemoveRecordAll();

            if( !ReadDir( pszNewDir ))
                ReadDir( _strCurDir.c_str() );
            else
                _strCurDir = pszNewDir;

            free( pszNewDir );
        }
    }
    else
        KSCPDownload();

    return 0;
}

MRESULT KSCPClient::CnEdit( USHORT usNotifyCode, ULONG ulParam )
{
    PCNREDITDATA pced = reinterpret_cast< PCNREDITDATA >( ulParam );
    PKSCPRECORD  pkr  = reinterpret_cast< PKSCPRECORD >( pced->pRecord );
    PFIELDINFO   pfi  = pced->pFieldInfo;

    if( !pkr || !pfi || pfi->offStruct != FIELDOFFSET( KSCPRECORD, pszName ))
        return 0;

    switch( usNotifyCode )
    {
        case CN_BEGINEDIT :
            if( !strcmp(  pkr->pszName, ".."))
                _kcnr.CloseEditP();
            break;

        case CN_REALLOCPSZ :
            // Does not free pszName here in order to use mrc.pszIcon as
            // an old name on CN_ENDEDIT. Then free mrc.pszIcon there

            pkr->pszName = new char[ pced->cbText ];
            if( pkr->pszName )
                return MRFROMLONG( true );
            break;

        case CN_ENDEDIT :
            // If an user cancel direct editing, CN_REALLOCPSZ is not called.
            // So if pszName and mrc.pszIcon are same, it means that
            // an user canceled direct editing. In this case, we should not
            // call Rename(). Otherwise it causes double-free memory
            if( pkr->pszName != pkr->mrc.pszIcon )
                Rename( pkr );
            break;
    }

    return 0;
}

MRESULT KSCPClient::OnCreate( PVOID pCtrlData, PCREATESTRUCT pcs )
{
    char szStr[ CCHMAXPATH ];

    _sock         = -1;
    _session      = 0;
    _sftp_session = 0;
    _fBusy        = false;
    _fCanceled    = false;

    _strCurDir.clear();
    _strDlDir.clear();

    if( DosLoadModule( szStr, sizeof( szStr ), "pmwp", &_hmodPMWP ))
    {
        _hmodPMWP = NULLHANDLE;

        _hptrDefaultFile   = QuerySysPointer( SPTR_FILE, false );
        _hptrDefaultFolder = QuerySysPointer( SPTR_FOLDER, false );
    }
    else
    {
        _hptrDefaultFile   = LoadPointer( _hmodPMWP, 24 );
        _hptrDefaultFolder = LoadPointer( _hmodPMWP, 26 );
    }

    _kmenuPopup.LoadMenu( this, 0, IDM_KSCP_POPUP );

    PrfQueryProfileString( HINI_USERPROFILE, KSCP_PRF_APP, KSCP_PRF_KEY_DLDIR,
                           "", szStr, sizeof( szStr ));

    _strDlDir = szStr;

    return 0;
}

MRESULT KSCPClient::OnDestroy()
{
    KSCPDisconnect();

    PrfWriteProfileString( HINI_USERPROFILE, KSCP_PRF_APP, KSCP_PRF_KEY_DLDIR,
                           _strDlDir.c_str());

    _strDlDir.clear();

    if( _hmodPMWP )
    {
        DestroyPointer( _hptrDefaultFile );
        DestroyPointer( _hptrDefaultFolder );

        DosFreeModule( _hmodPMWP );
    }

    return 0;
}

MRESULT KSCPClient::OnControl( USHORT id, USHORT usNotifyCode,
                               ULONG ulControlSpec )
{
    switch( usNotifyCode )
    {
        case CN_CONTEXTMENU :
            return CnContextMenu();

        case CN_ENTER :
            return CnEnter( ulControlSpec );

        case CN_BEGINEDIT :
        case CN_REALLOCPSZ :
        case CN_ENDEDIT :
            return CnEdit( usNotifyCode, ulControlSpec );
    }

    return 0;
}

MRESULT KSCPClient::OnPaint()
{
    KWindowPS kwps;

    RECTL rcl;

    kwps.BeginPaint( this, 0, &rcl );
    kwps.FillRect( &rcl, SYSCLR_WINDOW );
    kwps.EndPaint();

    return 0;
}

MRESULT KSCPClient::OnSize( SHORT scxOld, SHORT scyOld,
                            SHORT scxNew, SHORT scyNew )
{
    _kcnr.SetWindowPos( KWND_TOP, 0, 0, scxNew, scyNew, SWP_SIZE );

    return MRFROMLONG( true );
}

MRESULT KSCPClient::CmdSrcMenu( USHORT usCmd, bool fPointer )
{
    switch( usCmd )
    {
        case IDM_FILE_OPEN  :
            FileOpen();
            break;

        case IDM_FILE_ADDRBOOK :
            FileAddrBook();
            break;

        case IDM_FILE_CLOSE :
            FileClose();
            break;

        case IDM_FILE_DLDIR :
            FileDlDir();
            break;

        case IDM_FILE_EXIT  :
            PostMsg( WM_QUIT );
            break;

        case IDM_KSCP_DOWNLOAD :
            KSCPDownload();
            break;

        case IDM_KSCP_UPLOAD :
            KSCPUpload();
            break;

        case IDM_KSCP_DELETE :
            KSCPDelete();
            break;

        case IDM_KSCP_RENAME :
            KSCPRename();
            break;
    }

    return 0;
}

