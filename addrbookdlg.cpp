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

#include <KPMLib.h>

#include "kscp.h"

#include "KAddrBookDlg.h"

bool abDlg( KWindow* pkwndO, PSERVERINFO psiResult )
{
    KAddrBookDlg kabd;

    kabd.DlgBox( KWND_DESKTOP, pkwndO, 0, IDD_ADDRBOOK, psiResult );

    return ( kabd.GetResult() == DID_OK ) ? true : false;
}

bool getServerInfo( KWindow* pkwndO, PSERVERINFO psi, bool fSet )
{
    KDialog kdlg;
    ULONG ulReply = DID_CANCEL;

    if( kdlg.LoadDlg( KWND_DESKTOP, pkwndO, 0, IDD_OPEN ))
    {
        KComboBox   kcbAddr;
        KEntryField kefUserName;
        KEntryField kefPassword;
        KEntryField kefDir;
        KComboBox   kcbAuth;

        kdlg.WindowFromID( IDCB_OPEN_ADDR, kcbAddr );
        kdlg.WindowFromID( IDEF_OPEN_USERNAME, kefUserName );
        kdlg.WindowFromID( IDEF_OPEN_PASSWORD, kefPassword );
        kdlg.WindowFromID( IDEF_OPEN_DIRECTORY, kefDir );
        kdlg.WindowFromID( IDCB_OPEN_AUTHENTICATION, kcbAuth );

        kefUserName.SetTextLimit( sizeof( psi->szUserName ));
        kefPassword.SetTextLimit( sizeof( psi->szPassword ));
        kefDir.SetTextLimit( sizeof( psi->szDir ));

        kcbAuth.LmInsertItem( LIT_END, "Password");
        kcbAuth.LmInsertItem( LIT_END, "Public key(RSA)");
        kcbAuth.LmInsertItem( LIT_END, "Public key(DSA)");
        kcbAuth.LmSelectItem( 0, TRUE );

        if( fSet )
        {
            kcbAddr.SetWindowText( psi->szAddress );
            kefUserName.SetWindowText( psi->szUserName );
            kefPassword.SetWindowText( psi->szPassword );
            kefDir.SetWindowText( psi->szDir );
            kcbAuth.LmSelectItem( psi->iAuth, TRUE );
        }

        kdlg.ProcessDlg();

        ulReply = kdlg.GetResult();
        if( ulReply == DID_OK )
        {
            int len;

            kcbAddr.QueryWindowText( sizeof( psi->szAddress ),
                                     psi->szAddress );

            kefUserName.QueryWindowText( sizeof( psi->szUserName ),
                                          psi->szUserName );

            kefPassword.QueryWindowText( sizeof( psi->szPassword ),
                                          psi->szPassword );

            kefDir.QueryWindowText( sizeof( psi->szDir ), psi->szDir );

            len = strlen( psi->szDir );
            if( !len || psi->szDir[ len - 1 ] != '/')
                strcat( psi->szDir, "/");

            psi->iAuth = kcbAuth.LmQuerySelection( LIT_FIRST );

            if( !psi->szAddress[ 0 ])
                ulReply = DID_CANCEL;
        }

        kdlg.DestroyWindow();
    }

    return ( ulReply == DID_OK ) ? true : false;
}

