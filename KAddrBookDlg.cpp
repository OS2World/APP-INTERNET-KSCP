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

#define INCL_WIN
#include <os2.h>

#include "kscp.h"

#include "KAddrBookDlg.h"

MRESULT KAddrBookDlg::OnInitDlg( HWND hwndFocus, PVOID pCreate )
{
    SWP  swp;
    LONG cxScreen, cyScreen;

    bool  fShow = false;
    ULONG ulBufMax;

    WindowFromID( IDST_ADDRBOOK_SERVERS, _kstServers );
    WindowFromID( IDLB_ADDRBOOK_SERVERS, _klbServers );
    WindowFromID( IDPB_ADDRBOOK_ADD, _kbtnAdd );
    WindowFromID( IDPB_ADDRBOOK_EDIT, _kbtnEdit );
    WindowFromID( IDPB_ADDRBOOK_REMOVE, _kbtnRemove );
    WindowFromID( IDCB_ADDRBOOK_SHOW, _kbtnShow );

    QueryWindowPos( &swp );
    cxScreen = QuerySysValue( SV_CXSCREEN );
    cyScreen = QuerySysValue( SV_CYSCREEN );

    swp.x = ( cxScreen - swp.cx ) / 2;
    swp.y = ( cyScreen - swp.cy ) / 2;

    SetWindowPos( KWND_TOP, swp.x, swp.y, 0, 0, SWP_MOVE );

    _siv.Load();

    _psiResult = reinterpret_cast< PSERVERINFO >( pCreate );

    for( int i = 0; i < _siv.Count(); ++i )
        _klbServers.InsertItem( LIT_END, _siv.QueryServer( i )->strAddress );

    ulBufMax = sizeof( fShow );
    PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                         KSCP_PRF_KEY_SHOW, &fShow, &ulBufMax );

    _kbtnShow.SetCheck( fShow );

    return MRFROMLONG( false );
}

MRESULT KAddrBookDlg::OnDestroy()
{
    bool fShow;

    fShow = QueryButtonCheckstate( IDCB_ADDRBOOK_SHOW );

    PrfWriteProfileData( HINI_USERPROFILE, KSCP_PRF_APP, KSCP_PRF_KEY_SHOW,
                         &fShow, sizeof( fShow ));

    _siv.Save();

    return 0;
}

MRESULT KAddrBookDlg::OnControl( USHORT id, USHORT usNotify, ULONG ulParam )
{
    if( usNotify == LN_ENTER &&
        _klbServers.QueryLboxSelectedItem() >= 0 )
        PostMsg( WM_COMMAND, MPFROMSHORT( DID_OK ),
                 MPFROM2SHORT( CMDSRC_PUSHBUTTON, 0 ));

    return 0;
}

MRESULT KAddrBookDlg::CmdSrcPushButton( USHORT usCmd, bool fPointer )
{
    switch( usCmd )
    {
        case IDPB_ADDRBOOK_ADD :
            Add();
            break;

        case IDPB_ADDRBOOK_EDIT :
            Edit();
            break;

        case IDPB_ADDRBOOK_REMOVE :
            Remove();
            break;

        case DID_OK :
            Ok();
            break;

        case DID_CANCEL :
            Cancel();
            break;
    }

    _klbServers.SetFocus();

    return 0;
}

void KAddrBookDlg::Add()
{
    PSERVERINFO psi = new SERVERINFO();

    if( getServerInfo( this, psi, false ))
    {
        if( psi->strAddress[ 0 ])
        {
            _klbServers.InsertLboxItem( LIT_END, psi->strAddress );
            _siv.Add( psi );

            return;
        }
    }

    delete psi;
}

LONG KAddrBookDlg::CheckLboxIndex()
{
    LONG lIndex;

    lIndex = _klbServers.QueryLboxSelectedItem();
    if( lIndex < 0 )
    {
        MessageBox("Please selecte a server", "Addres Book",
                   MB_OK | MB_INFORMATION );
    }

    return lIndex;
}

void KAddrBookDlg::Edit()
{
    LONG lIndex;

    lIndex = CheckLboxIndex();
    if( lIndex >= 0 )
    {
        PSERVERINFO psiTemp = new SERVERINFO;

        *psiTemp = *_siv.Search( lIndex );
        if( getServerInfo( this, psiTemp, true ))
        {
            if( psiTemp->strAddress[ 0 ])
            {
                _klbServers.SetLboxItemText( lIndex, psiTemp->strAddress );

                _siv.Replace( lIndex, psiTemp );

                return;
            }
        }

        delete psiTemp;
    }
}

void KAddrBookDlg::Remove()
{
    LONG lIndex;

    lIndex = CheckLboxIndex();
    if( lIndex >= 0 )
    {
        if( MessageBox("Are you sure ?", "Address Book",
                       MB_YESNO | MB_QUERY ) == MBID_YES )
        {
            _siv.Remove( lIndex );

            _klbServers.DeleteLboxItem( lIndex );
        }
    }
}

void KAddrBookDlg::Ok()
{
    LONG lIndex;

    lIndex = CheckLboxIndex();
    if( lIndex >= 0 )
    {
        *_psiResult = *_siv.Search( lIndex );

        DismissDlg( DID_OK );
    }
}

void KAddrBookDlg::Cancel()
{
    DismissDlg( DID_CANCEL );
}

