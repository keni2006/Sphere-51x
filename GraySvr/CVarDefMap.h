#ifndef _INC_CVARDEFMAP_H
#define _INC_CVARDEFMAP_H
#pragma once

#include "../Common/common.h"
#include "../Common/cstring.h"
#include "../Common/cscript.h"

#include <vector>

class CVarDefCont : public CMemDynamic
{
private:
        CGString m_sKey;
        CGString m_sVal;
        long m_iVal;
        bool m_fIsNum;
        bool m_fQuoted;

public:
        CVarDefCont( LPCTSTR pszKey );

        const TCHAR * GetKey() const;
        void SetKey( LPCTSTR pszKey );

        const TCHAR * GetValStr() const;
        long GetValNum() const;

        void SetValStr( LPCTSTR pszVal, bool fQuoted );
        void SetValNum( long iVal );

        bool IsNum() const { return( m_fIsNum ); }
        bool IsQuoted() const { return( m_fQuoted ); }

        CVarDefCont * CopySelf() const;
};

class CVarDefMap
{
private:
        typedef std::vector<CVarDefCont*> EntryList;
        EntryList m_Entries;

private:
        static CGString NormalizeKey( LPCTSTR pszKey );
        static void ToLower( CGString & sKey );
        static bool TryParseNumber( LPCTSTR pszVal, long & lVal );
        static bool ShouldWriteQuoted( const CVarDefCont * pVar );

        CVarDefCont * Find( LPCTSTR pszKey ) const;
        CVarDefCont * AddNew( const CGString & sKey );
        void Empty();

public:
        CVarDefMap();
        ~CVarDefMap();

        void Copy( const CVarDefMap * pOther );
        CVarDefMap & operator = ( const CVarDefMap & other );

        size_t GetCount() const;
        CVarDefCont * GetAt( size_t index ) const;
        CVarDefCont * GetKey( LPCTSTR pszKey ) const;

        long GetKeyNum( LPCTSTR pszKey ) const;
        LPCTSTR GetKeyStr( LPCTSTR pszKey, bool fZero = false ) const;

        CVarDefCont * SetNum( LPCTSTR pszKey, long lVal, bool fZero = false );
        CVarDefCont * SetStr( LPCTSTR pszKey, bool fQuoted, LPCTSTR pszVal, bool fZero = false );

        void DeleteKey( LPCTSTR pszKey );
        void ClearKeys( LPCTSTR pszMask = NULL );

        void DumpKeys( CTextConsole * pSrc, LPCTSTR pszPrefix = NULL ) const;
        bool r_LoadVal( CScript & s );
        void r_WritePrefix( CScript & s, LPCTSTR pszPrefix = NULL, LPCTSTR pszExclude = NULL );
};

#endif // _INC_CVARDEFMAP_H
