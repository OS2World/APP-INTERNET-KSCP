#define INCL_WIN
#define INCL_DOS
#include <os2.h>

#include <iostream>

#include <ctime>
#include <sstream>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <KDirDlg.h>

#include "kscp.h"

#include "KRemoteWorkThread.h"
#include "KLocalWorkThread.h"
#include "KConnectThread.h"

#include "KSCPClient.h"

#define IDC_CONTAINER   1

#define KSCP_PRF_KEY_DLDIR  "DownloadDir"

void KSCPClient::QuerySSHHome( string& strHome )
{
    ULONG        ulBootDrive;

    DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                     &ulBootDrive, sizeof( ulBootDrive ));

    strHome  = static_cast< char >( ulBootDrive + 'A' - 1 );
    strHome += ":/os2/.ssh";

    struct stat statbuf;
    if( stat( strHome.c_str(), &statbuf ) || !S_ISDIR( statbuf.st_mode ))
    {
        const char* pszHome = getenv("HOME");
        if( !pszHome )
            pszHome = ".";

        strHome  = pszHome;
        strHome += "/.ssh";
    }
}

bool KSCPClient::CheckHostkey( PSERVERINFO psi )
{
    string              strKnownHostFile;
    LIBSSH2_KNOWNHOSTS* nh;
    struct              libssh2_knownhost* host;
    int                 hostcount;
    const char*         hostkey;
    const char*         fingerprint;
    size_t              hostkeylen;
    int                 type;
    int                 check;
    char*               errmsg;
    stringstream        sstMsg;
    bool                rc = false;

    hostkey = libssh2_session_hostkey( _session, &hostkeylen, &type );
    if( !hostkey )
    {
        libssh2_session_last_error( _session, &errmsg, NULL, 0 );

        sstMsg << "Failed to query a hostkey :" << endl
               << errmsg;
        MessageBox( sstMsg.str(), psi->strAddress, MB_OK | MB_ERROR );

        return rc;
    }

    nh = libssh2_knownhost_init( _session );
    if( !nh )
    {
        libssh2_session_last_error( _session, &errmsg, NULL, 0 );

        sstMsg << "Failed to initialize a known host :" << endl
               << errmsg;
        MessageBox( sstMsg.str(), psi->strAddress, MB_OK | MB_ERROR );

        return rc;
    }

    QuerySSHHome( strKnownHostFile );

    strKnownHostFile += "/known_hosts";
    hostcount = libssh2_knownhost_readfile( nh, strKnownHostFile.c_str(),
                                           LIBSSH2_KNOWNHOST_FILE_OPENSSH );

    check = libssh2_knownhost_check( nh, psi->strAddress.c_str(),
                                     hostkey, hostkeylen,
                                     LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                     LIBSSH2_KNOWNHOST_KEYENC_RAW, &host );

    fingerprint = libssh2_hostkey_hash( _session, LIBSSH2_HOSTKEY_HASH_MD5 );
    if( fingerprint )
    {
        for( int i = 0; i < 16; i++ )   // MD5 hash size is 16 bytes
        {
            sstMsg.width( 2 );
            sstMsg.fill('0');
            sstMsg << hex << uppercase
                   << static_cast< unsigned >
                        ( static_cast< unsigned char > ( fingerprint[ i ]));

            sstMsg.width();
            sstMsg << ":";
        }
        sstMsg << endl;
    }

    switch( check )
    {
        case LIBSSH2_KNOWNHOST_CHECK_FAILURE :
            sstMsg << "Hostkey check failed." << endl
                   << "Are you sure to continue connecting ?";
            break;

        case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND :
            sstMsg << "Hostkey not found." << endl
                   << "Are you sure to accept this key "
                   << "and to continue connecting ?";
            break;

        case LIBSSH2_KNOWNHOST_CHECK_MATCH :
            rc = true;
            break;

        case LIBSSH2_KNOWNHOST_CHECK_MISMATCH :
            sstMsg << "Hostkey mismatched." << endl
                   << "This MAY be a HACKING." << endl
                   << "If you want to continue to connecting, "
                   << "remove a line for this host in your known host file, "
                   << strKnownHostFile;

            break;
    }

    if( check != LIBSSH2_KNOWNHOST_CHECK_MATCH )
        rc = MessageBox( sstMsg.str(), psi->strAddress,
                         check != LIBSSH2_KNOWNHOST_CHECK_MISMATCH ?
                            ( MB_YESNO | MB_QUERY ) :
                            ( MB_OK | MB_WARNING )) == MBID_YES;

    if( rc && check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND )
    {
        sstMsg.str("");

        switch( type )
        {
            case LIBSSH2_HOSTKEY_TYPE_RSA :
                type = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
                break;

            case LIBSSH2_HOSTKEY_TYPE_DSS :
                type =  LIBSSH2_KNOWNHOST_KEY_SSHDSS;
                break;
        }

        if( libssh2_knownhost_addc( nh, psi->strAddress.c_str(), NULL,
                                    hostkey, hostkeylen, NULL, 0,
                                    LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                    LIBSSH2_KNOWNHOST_KEYENC_RAW |
                                    type, NULL ))
        {
            libssh2_session_last_error( _session, &errmsg, NULL, 0 );
            sstMsg << "Failed to add a hostkey :" << endl
                   << errmsg;

            MessageBox( sstMsg.str(), psi->strAddress, MB_OK | MB_ERROR );
        }
        else if( libssh2_knownhost_writefile( nh, strKnownHostFile.c_str(),
                                              LIBSSH2_KNOWNHOST_FILE_OPENSSH ))
        {
            libssh2_session_last_error( _session, &errmsg, NULL, 0 );
            sstMsg << "Failed to write to "
                   << strKnownHostFile << " :" << endl
                   << errmsg;

            MessageBox( sstMsg.str(), psi->strAddress, MB_OK | MB_ERROR );
        }
    }

    libssh2_knownhost_free( nh );

    return rc;
}

bool KSCPClient::ReadDir( const string& strDir )
{
    LIBSSH2_SFTP_HANDLE* sftp_handle;
    PKSCPRECORD          pkr;
    RECORDINSERT         ri;
    ULONG                ulStyle;
    int                  rc;

    cerr << "libssh2_sftp_opendir()!" << endl;
    /* Request a dir listing via SFTP */
    sftp_handle = libssh2_sftp_opendir( _sftp_session, strDir.c_str());

    if (!sftp_handle)
    {
        stringstream sstMsg;
        char *errmsg;

        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg << "Unable to open dir with SFTP :" << endl
               << errmsg;

        MessageBox( sstMsg.str(), "OpenDir",  MB_OK | MB_ERROR );

        return false;
    }
    cerr << "libssh2_sftp_opendir() is done, now receive listing!" << endl;
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

            pkr->pszSize = 0;

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

                asprintf( &pkr->pszSize, "%.*f %s",
                          usUnit == 0 ? 0 : 2, dSize, pszUnit[ usUnit ]);
            }


            pkr->pszDate = 0;

            if( strcmp( pkr->pszName, "..") &&
                attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME )
            {
               struct tm tm = *localtime( reinterpret_cast< time_t* >
                                            ( &attrs.mtime ));

               asprintf( &pkr->pszDate, "%04d-%02d-%02d, %02d:%02d",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min );
            }

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

void KSCPClient::ConnectMain( u_long to_addr, int port, int timeout,
                              HEV hevDone, int* piResult )
{
    int    flags;
    struct sockaddr_in sin;
    fd_set rset, wset;
    struct timeval tv;
    int    rc;

    flags = fcntl( _sock, F_GETFL );
    fcntl( _sock, F_SETFL, flags | O_NONBLOCK );

    sin.sin_family = AF_INET;
    sin.sin_port = htons( port );
    sin.sin_addr.s_addr = to_addr;
    if( connect( _sock, reinterpret_cast< struct sockaddr* >( &sin ),
                 sizeof( struct sockaddr_in )) != 0 )
    {
        if( sock_errno() != EINPROGRESS )
        {
            rc = sock_errno();
            goto exit_set_fl;
        }
    }
    else
    {
        rc = 0;
        goto exit_set_fl;
    }

    /* Support positive timeout only */
    for(; !_fCanceled && timeout > 0 ; timeout -= 100 )
    {
        FD_ZERO( &rset );
        FD_SET( _sock, &rset );
        wset = rset;

        tv.tv_sec  = 0;
        tv.tv_usec = 100 * 1000;

        rc = select( _sock + 1, &rset, &wset, NULL, &tv );
        if( rc != 0 )
            break;
    }

    if( rc == 0 )
        rc = ETIMEDOUT;
    else if( rc < 0 )
        rc = sock_errno();
    else
        rc = 0;

exit_set_fl :
    fcntl( _sock, F_SETFL, flags );

    *piResult = _fCanceled ? -1 : rc;

    _kdlg.DismissDlg( _fCanceled ? DID_CANCEL : DID_OK );

    DosPostEventSem( hevDone );
}

int KSCPClient::Connect( u_long to_addr, int port, int timeout )
{
    HEV hevDone;
    int iResult;

    _fCanceled = false;

    DosCreateEventSem( NULL, &hevDone, 0, FALSE );

    _kdlg.LoadDlg( KWND_DESKTOP, this, 0, IDD_DOWNLOAD );
    _kdlg.Centering();
    _kdlg.SetWindowText( _strAddress );
    _kdlg.SetDlgItemText( IDT_DOWNLOAD_INDEX, "");
    _kdlg.SetDlgItemText( IDT_DOWNLOAD_FILENAME, "");
    _kdlg.SetDlgItemText( IDT_DOWNLOAD_STATUS, "Connecting, please wait...");
    _kdlg.SetDlgItemText( IDT_DOWNLOAD_SPEED, "");

    void* apArg[] = { this, &to_addr, &port, &timeout, &hevDone, &iResult };

    KConnectThread thread;
    thread.BeginThread( apArg );

    _kdlg.ProcessDlg();
    if( _kdlg.GetResult() == DID_CANCEL )
        _fCanceled = true;

    _kdlg.DestroyWindow();

    WinWaitEventSem( hevDone, SEM_INDEFINITE_WAIT );
    DosCloseEventSem( hevDone );

    return iResult;
}

#define SSH_PORT        22
#define MAX_WAIT_TIME   ( 10 * 1000 )

bool KSCPClient::KSCPConnect( PSERVERINFO psi )
{
    stringstream       sstMsg;
    char*              errmsg;
    struct hostent*    host;
    PFIELDINFO         pfi, pfiStart;
    FIELDINFOINSERT    fii;
    CNRINFO            ci;
    int                rc;
    RECTL              rcl;

    KWindow kwnd;

    if( _sock != -1 )
        KSCPDisconnect();

    _strAddress = psi->strAddress;
    _strCurDir  = psi->strDir;

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    _sock = socket( AF_INET, SOCK_STREAM, 0 );

    host = gethostbyname( psi->strAddress.c_str());
    if( !host )
    {
        sstMsg << "Cannot resolve host " << psi->strAddress << " :" << endl
               << strerror( sock_errno());

        MessageBox( sstMsg.str(), "Connect", MB_OK | MB_ERROR );

        goto exit_close_socket;
    }

    rc = Connect( *reinterpret_cast< u_long* >( host->h_addr ), SSH_PORT,
                  MAX_WAIT_TIME );
    if( rc != 0 )
    {
        sstMsg << "Failed to connect to " << psi->strAddress << " :" << endl
               << ( rc > 0 ? strerror( rc ) : "Canceled");

        MessageBox( sstMsg.str(), "Connect", MB_OK | MB_ERROR );

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
        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg << "Failed to establish SSH session :" << endl
               << errmsg;
        MessageBox( sstMsg.str(), "Connect", MB_OK | MB_ERROR );

        goto exit_session_free;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    if( !CheckHostkey( psi ))
        goto exit_session_disconnect;

    if( psi->iAuth == 0 )
    {
        /* We could authenticate via password */
        if( libssh2_userauth_password( _session, psi->strUserName.c_str(),
                                       psi->strPassword.c_str()))
        {
            libssh2_session_last_error( _session, &errmsg, NULL, 0 );
            sstMsg << "Authentication by password failed :" << endl
                   << errmsg;
            MessageBox( sstMsg.str(), "Connect", MB_OK | MB_ERROR );

            goto exit_session_disconnect;
        }
    }
    else
    {
        string strHome;

        QuerySSHHome( strHome );

        stringstream sstPublicKey;
        stringstream sstPrivateKey;

        sstPublicKey << strHome << "/id_"
                     << ( psi->iAuth == 1 ? "rsa" : "dsa")
                     << ".pub";

        sstPrivateKey << strHome << "/id_"
                      << ( psi->iAuth == 1 ? "rsa" : "dsa" );

        /* Or by public key */
        if( libssh2_userauth_publickey_fromfile( _session,
                            psi->strUserName.c_str(),
                            sstPublicKey.str().c_str(),
                            sstPrivateKey.str().c_str(),
                            psi->strPassword.c_str() ))
        {
            libssh2_session_last_error( _session, &errmsg, NULL, 0 );
            sstMsg << "Authentication by public key failed :" << endl
                   << errmsg;
            MessageBox( sstMsg.str(), "Connect", MB_OK | MB_ERROR );

            goto exit_session_disconnect;
        }
    }

    cerr << "libssh2_sftp_init()!" << endl;
    _sftp_session = libssh2_sftp_init( _session );

    if( !_sftp_session )
    {
        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg << "Unable to init SFTP session :" << endl
               << errmsg;
        MessageBox( sstMsg.str(), "Connect", MB_OK | MB_ERROR );

        goto exit_session_disconnect;
    }

    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking( _session, 1 );

    _kcnr.CreateWindow( this, "",
                       CCS_AUTOPOSITION | CCS_EXTENDSEL | CCS_MINIICONS,
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

    if( !ReadDir( psi->strDir ))
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
    _strAddress.clear();

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

void KSCPClient::KSCPDisconnect( bool fQuery )
{
    if( _sock == -1 )
        return;

    if( fQuery )
    {
        if( MessageBox("Are you sure to disconnect this session?",
                       _strAddress, MB_YESNO | MB_QUERY ) ==
            MBID_NO )
            return;
    }

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
    _strAddress.clear();
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
    KSCPDisconnect( true );
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
    kdd.SetFullFile( _strDlDir );
    if( kdd.FileDlg( KWND_DESKTOP, this ) && kdd.GetReturn() == DID_OK )
        _strDlDir = kdd.GetFullFile();
}

void KSCPClient::FileExit()
{
    if( _sock != -1 )
    {
        if( MessageBox("You have a connection.\n"\
                       "Are you sure quit after disconnecting ?",
                        _strAddress, MB_YESNO | MB_QUERY ) !=
            MBID_YES )
            return;
    }

    PostMsg( WM_QUIT );
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

    ReadDir( _strCurDir );
}

#define BUF_SIZE    ( 1024 * 4 )

int KSCPClient::Download( PKSCPRECORD pkr )
{
    LIBSSH2_SFTP_HANDLE*     sftp_handle;
    string                   strSFTPPath;
    LIBSSH2_SFTP_ATTRIBUTES* pattr;

    struct stat       statbuf;
    FILE*             fp;
    char*             buf;
    libssh2_uint64_t  size;
    stringstream      sstMsg;

    struct timeval tv1, tv2;
    long long      diffTime;

    int rc = 1;

    strSFTPPath = _strCurDir + pkr->pszName;
    sftp_handle = libssh2_sftp_open( _sftp_session, strSFTPPath.c_str(),
                                     LIBSSH2_FXF_READ, 0 );
    if( !sftp_handle )
    {
        char* errmsg;

        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg << "Cannot open " << strSFTPPath << " :" << endl
               << errmsg;

        MessageBox( sstMsg.str(), "Download", MB_OK | MB_ERROR );

        return rc;
    }

    pattr = reinterpret_cast< LIBSSH2_SFTP_ATTRIBUTES* >( pkr->pbAttr );

    buf = new char[ BUF_SIZE ];
    _makepath( buf, NULL, _strDlDir.c_str(), pkr->pszName, NULL );

    if( !stat( buf, &statbuf ))
    {
        ULONG ulReply;

        sstMsg << buf << endl
               << "already exists. Overwrite ?";

        ulReply = MessageBox( sstMsg.str(), "Download",
                              MB_YESNO | MB_ICONQUESTION );

        if( ulReply == MBID_NO )
            goto exit_free;
    }

    fp = fopen( buf, "wb");
    if( !fp )
    {
        sstMsg.str("");
        sstMsg << "Cannot create " << buf << " :" << endl
               << strerror( errno );

        MessageBox( sstMsg.str(), "Download", MB_OK | MB_ERROR );

        goto exit_free;
    }

    if( pattr->filesize )
    {
        int nRead;

        for( size = diffTime = 0; !_fCanceled; )
        {
            sstMsg.str("");
            sstMsg << size / 1024 << " KB of "
                   << pattr->filesize / 1024 << " KB ("
                   << size * 100 / pattr->filesize << "%)";

            _kdlg.SetDlgItemText( IDT_DOWNLOAD_STATUS, sstMsg.str());

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
                sstMsg.str("");
                sstMsg << ( size * 1000000LL / 1024 ) / diffTime
                       << " KB/s";
                _kdlg.SetDlgItemText( IDT_DOWNLOAD_SPEED,
                                      sstMsg.str());
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

    stringstream sstMsg;

    int  count;
    int  i;

    _fBusy = true;

    count = CountRecords( CRA_SELECTED, false );

    for( i = 1, pkr = NULL; !_fCanceled; i++ )
    {
        pkr = FindRecord( pkr, CRA_SELECTED, false );
        if( !pkr )
            break;

        sstMsg.str("");
        sstMsg << i << " of " << count;
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_INDEX, sstMsg.str());
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_FILENAME, pkr->pszName );

        ( this->*pCallback )( pkr );
    }

    _kdlg.DismissDlg( _fCanceled ? DID_CANCEL : DID_OK );

    _fBusy = false;
}

int KSCPClient::KSCPDownload()
{
    stringstream sstMsg;

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
    _kdlg.Centering();

    RemoteParam rpParam = { this, &KSCPClient::Download };
    KRemoteWorkThread thread;
    thread.BeginThread( &rpParam );

    _kdlg.ProcessDlg();
    ulReply = _kdlg.GetResult();
    if( ulReply == DID_CANCEL )
        _fCanceled = true;

    _kdlg.DestroyWindow();

    sstMsg << "Download "
           << ( ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    MessageBox( sstMsg.str(), "Download", MB_OK | MB_INFORMATION );

    return 0;
}

int KSCPClient::Upload( const string& strName )
{
    LIBSSH2_SFTP_HANDLE*    sftp_handle;
    string                  strSFTPPath;
    LIBSSH2_SFTP_ATTRIBUTES sftp_attrs;

    FILE*        fp;
    off_t        size, fileSize;
    char*        buf;
    stringstream sstMsg;

    struct stat statbuf;

    struct timeval tv1, tv2;
    long long      diffTime;

    int rc = 1;

    if( stat( strName.c_str(), &statbuf ) < 0 || !S_ISREG( statbuf.st_mode ))
    {
        sstMsg << "Ooops... This is not a file. Ignored." << endl
               << strName;

        MessageBox( sstMsg.str(), "Upload", MB_OK | MB_ERROR );

        return rc;
    }

    fp = fopen( strName.c_str(), "rb");
    if( !fp )
    {
        sstMsg << "Cannot open " << strName << " :" << endl
               << strerror( errno );

        MessageBox( sstMsg.str(), "Upload", MB_OK | MB_ERROR );

        return rc;
    }

    fseeko( fp, 0, SEEK_END );
    fileSize = ftello( fp );
    fseeko( fp, 0, SEEK_SET );

    strSFTPPath  = _strCurDir;
    strSFTPPath += strName.find_last_of('\\') + 1;
    if( !libssh2_sftp_stat( _sftp_session, strSFTPPath.c_str(), &sftp_attrs ))
    {
        ULONG ulReply;

        sstMsg << strSFTPPath << endl
               << "already exists. Overwrite ?";

        ulReply = MessageBox( sstMsg.str() , "Upload",
                              MB_YESNO | MB_ICONQUESTION );

        if( ulReply == MBID_NO )
            goto exit_fclose;
    }

    sftp_handle = libssh2_sftp_open( _sftp_session, strSFTPPath.c_str(),
                                     LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT |
                                     LIBSSH2_FXF_TRUNC,
                                     LIBSSH2_SFTP_S_IRUSR |
                                     LIBSSH2_SFTP_S_IWUSR |
                                     LIBSSH2_SFTP_S_IRGRP |
                                     LIBSSH2_SFTP_S_IROTH );
    if( !sftp_handle )
    {
        char* errmsg;

        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg.str("");
        sstMsg << "Cannot create " << strSFTPPath << " :" << endl
               << errmsg;

        MessageBox( sstMsg.str(), "Upload", MB_OK | MB_ERROR );

        goto exit_fclose;
    }

    buf = new char[ BUF_SIZE ];

    if( fileSize )
    {
        int         nRead, nWrite;
        const char* ptr;

        for( size = diffTime = 0; !_fCanceled; )
        {
            sstMsg.str("");
            sstMsg << size / 1024 << " KB of "
                   << fileSize / 1024 << " KB ("
                   << size * 100 / fileSize << "%)";

            _kdlg.SetDlgItemText( IDT_DOWNLOAD_STATUS, sstMsg.str());

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
                sstMsg.str("");
                sstMsg << "Ooops... Error occurs while uploading" << endl
                       << strName;
                MessageBox( sstMsg.str(), "Upload",
                            MB_OK | MB_ERROR );
                goto exit_free;
            }

            if( diffTime )
            {
                sstMsg.str("");
                sstMsg << ( size * 1000000LL / 1024 ) / diffTime
                       << " KB/s";
                _kdlg.SetDlgItemText( IDT_DOWNLOAD_SPEED, sstMsg.str());
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
    KFDVECSTR vsList = _kfd.GetFQFilename();
    ULONG ulCount    = _kfd.GetFQFCount();

    stringstream sstMsg;
    unsigned     i;

    _fBusy = true;

    for( i = 0; i < ulCount && !_fCanceled; i++ )
    {
        sstMsg.str("");
        sstMsg << i + 1 << " of " << ulCount;
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_INDEX,  sstMsg.str());
        _kdlg.SetDlgItemText( IDT_DOWNLOAD_FILENAME, vsList[ i ]);
        (this->*pCallback)( vsList[ i ]);
    }

    _kdlg.DismissDlg( _fCanceled ? DID_CANCEL : DID_OK );

    _fBusy = false;
}

int KSCPClient::KSCPUpload()
{
    stringstream sstMsg;
    ULONG        ulReply;

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
    _kdlg.Centering();

    _kdlg.SetWindowText("Upload");

    LocalParam lpParam = { this, &KSCPClient::Upload };
    KLocalWorkThread thread;
    thread.BeginThread( &lpParam );

    _kdlg.ProcessDlg();
    ulReply = _kdlg.GetResult();
    if( ulReply == DID_CANCEL )
        _fCanceled = true;

    _kdlg.DestroyWindow();

    sstMsg << "Upload "
           << ( ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    MessageBox( sstMsg.str(), "Upload", MB_OK | MB_INFORMATION );

    Refresh();

    return 0;
}

int KSCPClient::Delete( PKSCPRECORD pkr )
{
    string       strSFTPPath;
    stringstream sstMsg;

    strSFTPPath = _strCurDir + pkr->pszName;
    if( libssh2_sftp_unlink( _sftp_session, strSFTPPath.c_str()))
    {
        char* errmsg;

        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg << "Cannot delete " << strSFTPPath << " :" << endl
               << errmsg;

        MessageBox( sstMsg.str(), "Delete", MB_OK | MB_ERROR );

        return 1;
    }

    return 0;
}

int KSCPClient::KSCPDelete()
{
    stringstream sstMsg;
    ULONG        ulReply;

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
    _kdlg.Centering();

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

    sstMsg << "Delete "
           << (ulReply == DID_CANCEL ? "CANCELED" : "COMPLETED");

    MessageBox( sstMsg.str(), "Delete", MB_OK | MB_INFORMATION );

    Refresh();

    return 0;
}

void KSCPClient::Rename( PKSCPRECORD pkr )
{
    string       strOldSFTPPath, strNewSFTPPath;
    stringstream sstMsg;

    strOldSFTPPath = _strCurDir + pkr->mrc.pszIcon;
    strNewSFTPPath = _strCurDir + pkr->pszName;
    if( strOldSFTPPath != strNewSFTPPath &&
        libssh2_sftp_rename( _sftp_session,
                             strOldSFTPPath.c_str(), strNewSFTPPath.c_str()))
    {
        char* errmsg;

        libssh2_session_last_error( _session, &errmsg, NULL, 0 );
        sstMsg << "Cannot rename " << pkr->mrc.pszIcon
               << " to " << pkr->pszName << " :" << endl
               << errmsg;

        MessageBox( sstMsg.str() , "Rename", MB_OK | MB_ERROR );

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
        string strNewDir;

        if( strcmp( pkr->pszName, ".."))
            strNewDir = _strCurDir + pkr->pszName + "/";
        else if( _strCurDir[ 1 ]) // not root ?
        {
            // remove the last '/'
           _strCurDir.erase( _strCurDir.end() - 1 );

            // remove the last directory part
            _strCurDir.erase( _strCurDir.find_last_of('/') + 1 );

            strNewDir = _strCurDir;
        }

        if( !strNewDir.empty())
        {
            RemoveRecordAll();

            if( !ReadDir( strNewDir ))
                ReadDir( _strCurDir );
            else
                _strCurDir = strNewDir;
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
            _fCnrEditing = true;

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

            _fCnrEditing = false;
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
    _fCnrEditing  = false;

    _strCurDir.clear();
    _strDlDir.clear();
    _strAddress.clear();

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

MRESULT KSCPClient::OnClose()
{
    PostMsg( WM_COMMAND, MPFROMSHORT( IDM_FILE_EXIT ),
             MPFROM2SHORT( CMDSRC_MENU, FALSE ));

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

MRESULT KSCPClient::OnTranslateAccel( PQMSG pqmsg )
{
    if( _fCnrEditing )
        return false;

    return KWindow::OnTranslateAccel( pqmsg );
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
            FileExit();
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

MRESULT KSCPClient::CmdSrcAccelerator( USHORT usCmd, bool fPointer )
{
    switch( usCmd )
    {
        case IDM_KSCP_UPLOAD :
            KSCPUpload();
            break;

        case IDM_KSCP_DELETE :
            KSCPDelete();
            break;
    }

    return 0;
}

