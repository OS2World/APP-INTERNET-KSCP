#ifndef KSCPRC_H
#define KSCPRC_H

#include <os2.h>

#define ID_KSCP  1

#define IDM_FILE            100
#define IDM_FILE_OPEN       101
#define IDM_FILE_ADDRBOOK   102
#define IDM_FILE_CLOSE      103
#define IDM_FILE_DLDIR      104
#define IDM_FILE_EXIT       105

#define IDM_KSCP_POPUP      200
#define IDM_KSCP_DOWNLOAD   201
#define IDM_KSCP_UPLOAD     202
#define IDM_KSCP_DELETE     210
#define IDM_KSCP_RENAME     211

#define IDD_OPEN                    1000
#define IDCB_OPEN_ADDR              1001
#define IDEF_OPEN_USERNAME          1002
#define IDEF_OPEN_PASSWORD          1003
#define IDEF_OPEN_DIRECTORY         1004
#define IDCB_OPEN_AUTHENTICATION    1005

#define IDD_DOWNLOAD            1100
#define IDT_DOWNLOAD_INDEX      1101
#define IDT_DOWNLOAD_FILENAME   1102
#define IDT_DOWNLOAD_STATUS     1103
#define IDT_DOWNLOAD_SPEED      1104

#define IDD_ADDRBOOK	1200
#define IDST_ADDRBOOK_SERVERS	1201
#define IDLB_ADDRBOOK_SERVERS	1202
#define IDPB_ADDRBOOK_ADD	1203
#define IDPB_ADDRBOOK_EDIT	1204
#define IDPB_ADDRBOOK_REMOVE	1205
#define IDCB_ADDRBOOK_SHOW	1206

#endif
