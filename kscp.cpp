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

#include <sstream>

#include <KPMLib.h>

#include <libssh2.h>

#include "KSCPClient.h"

#include "kscp.h"

int KSCP::Run()
{
    int   rc;

    rc = libssh2_init( 0 );
    if( rc != 0 )
    {
        stringstream ssMsg;

        ssMsg << "libssh2 initialization failed : rc = " << rc;

        MessageBox( KWND_DESKTOP, ssMsg.str(), "KSCP",  MB_OK | MB_ERROR );

        return 1;
    }

    KSCPClient kclient;
    kclient.RegisterClass( _hab, WC_KSCP, CS_SIZEREDRAW, sizeof( PVOID ));

    ULONG flFrameFlags;
    flFrameFlags = FCF_SYSMENU | FCF_TITLEBAR | FCF_MINMAX | FCF_SIZEBORDER |
                   FCF_SHELLPOSITION | FCF_TASKLIST | FCF_MENU |
                   FCF_ACCELTABLE;

    KFrameWindow kframe;
    kframe.CreateStdWindow( KWND_DESKTOP,           // parent window handle
                            WS_VISIBLE,             // frame window style
                            &flFrameFlags,          // window style
                            KSCP_TITLE,             // window title
                            0,                      // default client style
                            0,                      // resource in exe file
                            ID_KSCP,                // frame window id
                            kclient                 // client window handle
                          );

    if( kframe.GetHWND())
    {
        bool  fShow = false;
        ULONG ulBufMax;

        ulBufMax = sizeof( fShow );

        PrfQueryProfileData( HINI_USERPROFILE, KSCP_PRF_APP,
                             KSCP_PRF_KEY_SHOW, &fShow, &ulBufMax );

        if( fShow )
            kclient.PostMsg( WM_COMMAND, MPFROMSHORT( IDM_FILE_ADDRBOOK ),
                             MPFROM2SHORT( CMDSRC_MENU, false ));
        KPMApp::Run();

        kframe.DestroyWindow();
    }

    libssh2_exit();

    return 0;
}

int main( void )
{
    KSCP kscp;

    return kscp.Run();
}

