/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   KO Myung-Hun <komh@chollian.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define INCL_WIN
#include <os2.h>

#include <stdlib.h>
#include <string.h>

#include "windirdlg.h"

// KOMH: These codes are excerpted from /widget/os2/nsFilePicker.cpp
static
MRESULT EXPENTRY DirDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   switch ( msg ) {
      case WM_INITDLG:
         {
         SWP swpFileST;
         SWP swpDirST;
         SWP swpDirLB;
         SWP swpDriveST;
         SWP swpDriveCB;
         SWP swpDriveCBEF;
         SWP swpOK;
         SWP swpCancel;
         HWND hwndFileST;
         HWND hwndDirST;
         HWND hwndDirLB;
         HWND hwndDriveST;
         HWND hwndDriveCB;
         HWND hwndOK;
         HWND hwndCancel;
         HENUM henum;
         HWND hwndNext;
         ULONG ulCurY, ulCurX;
         LONG lScreenX, lScreenY, lDlgFrameX, lDlgFrameY, lTitleBarY;

         lScreenX = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
         lScreenY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
         lDlgFrameX = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
         lDlgFrameY = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
         lTitleBarY = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

         hwndFileST = WinWindowFromID(hwndDlg, DID_FILENAME_TXT);
         hwndDirST = WinWindowFromID(hwndDlg, DID_DIRECTORY_TXT);
         hwndDirLB = WinWindowFromID(hwndDlg, DID_DIRECTORY_LB);
         hwndDriveST = WinWindowFromID(hwndDlg, DID_DRIVE_TXT);
         hwndDriveCB = WinWindowFromID(hwndDlg, DID_DRIVE_CB);
         hwndOK = WinWindowFromID(hwndDlg, DID_OK);
         hwndCancel = WinWindowFromID(hwndDlg, DID_CANCEL);

#define SPACING 10
         // Reposition drives combobox
         ulCurY = SPACING;
         ulCurX = SPACING + lDlgFrameX;
         WinQueryWindowPos(hwndOK, &swpOK);
         WinSetWindowPos(hwndOK, 0, ulCurX, ulCurY, 0, 0, SWP_MOVE);
         ulCurY += swpOK.cy + SPACING;
         WinQueryWindowPos(hwndCancel, &swpCancel);
         WinSetWindowPos(hwndCancel, 0, ulCurX+swpOK.cx+10, SPACING, 0, 0, SWP_MOVE);
         WinQueryWindowPos(hwndDirLB, &swpDirLB);
         WinSetWindowPos(hwndDirLB, 0, ulCurX, ulCurY, swpDirLB.cx, swpDirLB.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpDirLB.cy + SPACING;
         WinQueryWindowPos(hwndDirST, &swpDirST);
         WinSetWindowPos(hwndDirST, 0, ulCurX, ulCurY, swpDirST.cx, swpDirST.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpDirST.cy + SPACING;
         WinQueryWindowPos(hwndDriveCB, &swpDriveCB);
         WinQueryWindowPos(WinWindowFromID(hwndDriveCB, CBID_EDIT), &swpDriveCBEF);
         WinSetWindowPos(hwndDriveCB, 0, ulCurX, ulCurY-(swpDriveCB.cy-swpDriveCBEF.cy)+5,
                                         swpDirLB.cx,
                                         swpDriveCB.cy,
                                         SWP_SIZE | SWP_MOVE);
         ulCurY += swpDriveCBEF.cy + SPACING;
         WinQueryWindowPos(hwndDriveST, &swpDriveST);
         WinSetWindowPos(hwndDriveST, 0, ulCurX, ulCurY, swpDriveST.cx, swpDriveST.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpDriveST.cy + SPACING;
         WinQueryWindowPos(hwndFileST, &swpFileST);
         WinSetWindowPos(hwndFileST, 0, ulCurX, ulCurY, swpFileST.cx, swpFileST.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpFileST.cy + SPACING;

         // Hide unused stuff
         henum = WinBeginEnumWindows(hwndDlg);
         while ((hwndNext = WinGetNextWindow(henum)) != NULLHANDLE)
         {
           USHORT usID = WinQueryWindowUShort(hwndNext, QWS_ID);
           if (usID != DID_FILENAME_TXT &&
               usID != DID_DIRECTORY_TXT &&
               usID != DID_DIRECTORY_LB &&
               usID != DID_DRIVE_TXT &&
               usID != DID_DRIVE_CB &&
               usID != DID_OK &&
               usID != DID_CANCEL &&
               usID != FID_TITLEBAR &&
               usID != FID_SYSMENU &&
               usID != FID_MINMAX)
           {
             WinShowWindow(hwndNext, FALSE);
           }
         }

         WinSetWindowPos(hwndDlg,
                         HWND_TOP,
                         (lScreenX/2)-((swpDirLB.cx+2*SPACING+2*lDlgFrameX)/2),
                         (lScreenY/2)-((ulCurY+2*lDlgFrameY+lTitleBarY)/2),
                         swpDirLB.cx+2*SPACING+2*lDlgFrameX,
                         ulCurY+2*lDlgFrameY+lTitleBarY,
                         SWP_MOVE | SWP_SIZE);

         // Give the focus to the directory list box
         WinPostMsg( hwndDlg, WM_CHAR, MPFROM2SHORT( KC_VIRTUALKEY, 0 ),
                     MPFROM2SHORT( 0, VK_TAB ));
         WinPostMsg( hwndDlg, WM_CHAR, MPFROM2SHORT( KC_VIRTUALKEY, 0 ),
                     MPFROM2SHORT( 0, VK_TAB ));
         }
         break;
      case WM_CONTROL:
         {
         PFILEDLG pfiledlg;
         pfiledlg = (PFILEDLG)WinQueryWindowPtr(hwndDlg, QWL_USER);

         HPS           hps;
         SWP           swp;
         HWND          hwndST;
         RECTL         rectlString = {0,0,1000,1000};
         char          *ptr = NULL;
         int           iHalfLen;
         int           iLength;
         CHAR          szString[CCHMAXPATH];

         hwndST = WinWindowFromID(hwndDlg, DID_FILENAME_TXT);

         strcpy(szString, pfiledlg->szFullFile);
         iLength = strlen(pfiledlg->szFullFile);
         /* If we are not just a drive */
         if (iLength > 3) {
           if (szString[iLength-1] == '\\') {
             szString[iLength-1] = '\0';
             iLength--;
           }
         }

         hps = WinGetPS(hwndST);
         WinQueryWindowPos(hwndST, &swp);

         WinDrawText(hps, iLength, szString,
                          &rectlString, 0, 0,
                          DT_BOTTOM | DT_QUERYEXTENT | DT_TEXTATTRS);
         while(rectlString.xRight > swp.cx)
         {
           iHalfLen = iLength / 2;
           if(iHalfLen == 2)
             break;

           ptr = szString + iHalfLen;
           memmove(ptr - 1, ptr, strlen(ptr) + 1);
           szString[iHalfLen - 2] = '.';
           szString[iHalfLen - 1] = '.';
           szString[iHalfLen]     = '.';
           iLength = strlen(szString);
           rectlString.xLeft = rectlString.yBottom = 0;
           rectlString.xRight = rectlString.yTop = 1000;
           WinDrawText(hps, iLength, szString,
                       &rectlString, 0, 0,
                       DT_BOTTOM | DT_QUERYEXTENT | DT_TEXTATTRS);
         }

         WinReleasePS(hps);
         WinSetWindowText(hwndST, szString);
         }
         break;
   }
   return WinDefFileDlgProc(hwndDlg, msg, mp1, mp2);
}

HWND WinDirDlg( HWND hwndP, HWND hwndO, PFILEDLG pfiledlg )
{
    HWND  hwndDlg;
    int   len = strlen( pfiledlg->szFullFile );
    char *p;

    if( !pfiledlg->cbSize )
         pfiledlg->cbSize = sizeof( FILEDLG );

    if( !pfiledlg->fl )
        pfiledlg->fl = FDS_CENTER | FDS_OPEN_DIALOG;

    if( !pfiledlg->pfnDlgProc )
        pfiledlg->pfnDlgProc = DirDialogProc;

    // Remove the trailing back-slash
    while( len > 0 && pfiledlg->szFullFile[ len - 1 ] == '\\' )
        len--;

    // To prevent from treating the last components of szFullFile as
    // a filename
    strncpy( pfiledlg->szFullFile + len, "\\^",
             sizeof( pfiledlg->szFullFile ) - len - 1 );

    hwndDlg = WinFileDlg( hwndP, hwndO, pfiledlg );

    // Remove a temporary filename part
    p = strrchr( pfiledlg->szFullFile, '\\');
    // Except x:\ case
    if( p && ( p - pfiledlg->szFullFile ) == 2 )
        p++;

    if( p )
        *p = '\0';

    return hwndDlg;
}
