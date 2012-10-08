#ifndef KSCP_CLIENT_H
#define KSCP_CLIENT_H

#define INCL_WIN
#include <os2.h>

#include <string>
using namespace std;

#include <KPMLib.h>

#include "addrbookdlg.h"

#include "KSCPContainer.h"

class KSCPClient : public KWindow
{
public :
    KSCPClient() : KWindow() {}
    virtual ~KSCPClient() {}

protected :
    virtual MRESULT OnControl( USHORT id, USHORT usNotifyCode,
                               ULONG ulControlSpec );
    virtual MRESULT OnCreate( PVOID pCtrlData, PCREATESTRUCT pcs );
    virtual MRESULT OnDestroy();
    virtual MRESULT OnPaint();
    virtual MRESULT OnSize( SHORT scxOld, SHORT scyOld,
                            SHORT scxNew, SHORT scyNew );

    virtual MRESULT CmdSrcMenu( USHORT usCmd, bool fPointer );
    virtual MRESULT CmdSrcAccelerator( USHORT usCmd, bool fPointer );

private :
    friend class KRemoteWorkThread;
    friend class KLocalWorkThread;

    KSCPContainer       _kcnr;
    KMenu               _kmenuPopup;
    int                 _sock;
    HMODULE             _hmodPMWP;
    HPOINTER            _hptrDefaultFile;
    HPOINTER            _hptrDefaultFolder;
    KDialog             _kdlg;
    KFileDlg            _kfd;
    LIBSSH2_SESSION*    _session;
    LIBSSH2_SFTP*       _sftp_session;
    string              _strCurDir;
    string              _strDlDir;
    bool                _fBusy;
    bool                _fCanceled;

    bool ReadDir( const char* dir );
    bool KSCPConnect( PSERVERINFO psi );

    void RemoveRecordAll();
    void KSCPDisconnect();

    bool FileOpen();
    void FileClose();
    bool FileAddrBook();
    void FileDlDir();

    PKSCPRECORD FindRecord( PKSCPRECORD pkrStart, ULONG ulEM, bool fWithDir );
    int         CountRecords( ULONG ulEM, bool fWithDir );

    void        Refresh();

    typedef int (KSCPClient::*PFN_REMOTE_CALLBACK )( PKSCPRECORD );

    struct RemoteParam
    {
        KSCPClient* pkscpc;
        PFN_REMOTE_CALLBACK pCallback;
    };

    void RemoteMain( PFN_REMOTE_CALLBACK pCallback );

    typedef int (KSCPClient::*PFN_LOCAL_CALLBACK )( const char* );

    struct LocalParam
    {
        KSCPClient* pkscpc;
        PFN_LOCAL_CALLBACK pCallback;
    };

    void LocalMain( PFN_LOCAL_CALLBACK pCallback );

    int Download( PKSCPRECORD pkr );
    int KSCPDownload();

    int Upload( const char*pszName );
    int KSCPUpload();

    int Delete( PKSCPRECORD pkr );
    int KSCPDelete();

    void Rename( PKSCPRECORD pkr );
    int  KSCPRename();

    MRESULT CnContextMenu();
    MRESULT CnEnter( ULONG ulParam );
    MRESULT CnEdit( USHORT usNotifyCode, ULONG ulParam );
};
#endif
