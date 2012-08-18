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

#ifndef KSCP_H
#define KSCP_H

#include <os2.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "windirdlg.h"
#include "addrbookdlg.h"

#include "kscprc.h"

#define KSCP_VERSION "0.3.0"

#define WC_KSCP  "KSCP CLASS"

#define KSCP_PRF_APP    "KSCP"

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

typedef struct _KSCPRECORD
{
    MINIRECORDCORE mrc;
    PSZ            pszName;
    PVOID          pAttr;
} KSCPRECORD, *PKSCPRECORD;

#define CB_EXTRA_KSCPRECORD \
    ( sizeof( KSCPRECORD ) - sizeof( MINIRECORDCORE ))

#ifdef DEBUG
#include <stdio.h>

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

#endif
