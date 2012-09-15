#ifndef SERVER_INFO_VECTOR_H
#define SERVER_INFO_VECTOR_H

#include <vector>
using namespace std;

#include "addrbookdlg.h"

class ServerInfoVector
{
public :
    ServerInfoVector() {}
    ~ServerInfoVector();

    void Load();
    void Save();

    int Count() const { return _vtServerInfo.size(); }

    const PSERVERINFO Search( LONG lIndex ) const
    { return _vtServerInfo[ lIndex ]; }

    void Add( const PSERVERINFO psi )
    { _vtServerInfo.push_back( psi ); }

    void Remove( LONG lIndex )
    {
        delete _vtServerInfo[ lIndex ];

        _vtServerInfo.erase( _vtServerInfo.begin() + lIndex );
    }

    const PSERVERINFO QueryServer( LONG lIndex ) const
    { return _vtServerInfo[ lIndex ]; }

    void Replace( LONG lIndex, const PSERVERINFO psiNew )
    {
        delete _vtServerInfo[ lIndex ];

        _vtServerInfo[ lIndex ] = psiNew;
    }

private :
    vector< PSERVERINFO > _vtServerInfo;
};
#endif
