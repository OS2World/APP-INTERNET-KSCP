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

#define KSCP_VERSION "0.1.0"

#define ID_KSCP  1
#define WC_KSCP  "KSCP CLASS"

#define IDM_FILE        100
#define IDM_FILE_OPEN   101
#define IDM_FILE_CLOSE  102
#define IDM_FILE_DLDIR  103
#define IDM_FILE_EXIT   104

#define IDM_KSCP_POPUP      200
#define IDM_KSCP_DOWNLOAD   201
#define IDM_KSCP_UPLOAD     202

#define IDD_OPEN            1000
#define IDCB_OPEN_ADDR      1001
#define IDEF_OPEN_USERNAME  1002
#define IDEF_OPEN_PASSWORD  1003

#define IDD_DOWNLOAD            1100
#define IDT_DOWNLOAD_INDEX      1101
#define IDT_DOWNLOAD_FILENAME   1102
#define IDT_DOWNLOAD_STATUS     1103
#define IDT_DOWNLOAD_SPEED      1104

#define IDD_DLDIR           1200
#define IDT_DLDIR_DIRNAME   1201

#endif

