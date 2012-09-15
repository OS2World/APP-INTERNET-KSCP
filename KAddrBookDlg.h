#ifndef KADDR_BOOK_DLG_H
#define KADDR_BOOK_DLG_H

#include <os2.h>

#include <KPMLib.h>

#include "ServerInfoVector.h"

class KAddrBookDlg : public KDialog
{
public :
    KAddrBookDlg() : KDialog() {}
    ~KAddrBookDlg() {}

protected :
    virtual MRESULT OnControl( USHORT id, USHORT usNotify, ULONG ulParam );
    virtual MRESULT OnDestroy();
    virtual MRESULT OnInitDlg( HWND hwndFocus, PVOID pCreate );

    virtual MRESULT CmdSrcPushButton( USHORT usCmd, bool fPointer );

private :
    KStaticText _kstServers;
    KListBox    _klbServers;
    KButton     _kbtnAdd;
    KButton     _kbtnEdit;
    KButton     _kbtnRemove;
    KButton     _kbtnShow;

    ServerInfoVector _siv;
    PSERVERINFO      _psiResult;

    void Add();
    LONG CheckLboxIndex();
    void Edit();
    void Remove();
    void Ok();
    void Cancel();
};
#endif
