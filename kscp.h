#ifndef KSCP_H
#define KSCP_H

#include <os2.h>

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

