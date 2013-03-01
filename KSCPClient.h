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
    virtual MRESULT OnClose();
    virtual MRESULT OnControl( USHORT id, USHORT usNotifyCode,
                               ULONG ulControlSpec );
    virtual MRESULT OnCreate( PVOID pCtrlData, PCREATESTRUCT pcs );
    virtual MRESULT OnDestroy();
    virtual MRESULT OnPaint();
    virtual MRESULT OnSize( SHORT scxOld, SHORT scyOld,
                            SHORT scxNew, SHORT scyNew );
    virtual MRESULT OnTranslateAccel( PQMSG pqmsg );

    virtual MRESULT CmdSrcMenu( USHORT usCmd, bool fPointer );
    virtual MRESULT CmdSrcAccelerator( USHORT usCmd, bool fPointer );

private :
    friend class KWorkerThread;

    KFrameWindow        _kframe;
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
    string              _strAddress;
    bool                _fCnrEditing;
    int                 _iResult;

    typedef void ( KSCPClient::*PFN_WORKER )( void* arg );

    struct WorkerParam
    {
        KSCPClient* pkscpc;
        PFN_WORKER  pfnWorker;
        void*       arg;
    };

    int  CallWorker( const string& strTitle, PFN_WORKER pfnWorker,
                     void* arg = 0 );

    void QuerySSHHome( string& strHome );
    bool CheckHostkey();
    void ReadDirWorker( void* arg );
    bool ReadDir( const string& strDir, const string& strSelected = "" );
    int  ConnectEx( u_long to_addr, int port, int timeout );
    void ConnectWorker( void* arg );
    bool Connect( PSERVERINFO psi );
    bool KSCPConnect( PSERVERINFO psi, bool fQuery = true );

    void RemoveRecordAll();
    void KSCPDisconnect( bool fQuery = false );

    bool FileOpen();
    void FileClose();
    bool FileAddrBook();
    void FileDlDir();
    void FileExit();

    PKSCPRECORD FindRecord( PKSCPRECORD pkrStart, ULONG ulEM, bool fWithDir );
    int         CountRecords( ULONG ulEM, bool fWithDir );

    void        Refresh();

    typedef int (KSCPClient::*PFN_REMOTE_CALLBACK )( PKSCPRECORD );

    struct RemoteParam
    {
        PFN_REMOTE_CALLBACK pCallback;
    };

    void RemoteWorker( void* arg );

    typedef int (KSCPClient::*PFN_LOCAL_CALLBACK )( const string& );

    struct LocalParam
    {
        PFN_LOCAL_CALLBACK pCallback;
    };

    void LocalWorker( void* arg );

    int Download( PKSCPRECORD pkr );
    int KSCPDownload();

    int Upload( const string& strName );
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
