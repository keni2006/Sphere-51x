#include "graysvr.h"
#include "CVarDefMap.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace
{
        static const TCHAR * k_szZero = "0";
        static const TCHAR * k_szEmpty = "";
}

static long StrToLong( const TCHAR * pszVal )
{
        if ( pszVal == NULL )
                return 0;
        return strtol( pszVal, NULL, 0 );
}

static void AppendKeyToList( CGString & sList, const CVarDefCont * pVar )
{
        if ( pVar == NULL )
                return;
        if ( !sList.IsEmpty())
        {
                sList += ",";
        }
        sList += pVar->GetKey();
}

/***************************************************************************/
// CVarDefCont
/***************************************************************************/

CVarDefCont::CVarDefCont( LPCTSTR pszKey ) :
        m_sKey( pszKey ? pszKey : k_szEmpty ),
        m_iVal( 0 ),
        m_fIsNum( false ),
        m_fQuoted( false )
{
        SetKey( m_sKey );
        m_sVal = k_szEmpty;
}

const TCHAR * CVarDefCont::GetKey() const
{
        return m_sKey.GetPtr();
}

void CVarDefCont::SetKey( LPCTSTR pszKey )
{
        if ( pszKey == NULL )
        {
                m_sKey.Empty();
                return;
        }
        m_sKey = pszKey;
        TCHAR * pBuffer = m_sKey.GetBuffer();
        if ( pBuffer == NULL )
                return;
        for ( size_t i = 0; pBuffer[i] != '\0'; ++i )
        {
                pBuffer[i] = static_cast<TCHAR>( tolower( static_cast<unsigned char>( pBuffer[i] )));
        }
}

const TCHAR * CVarDefCont::GetValStr() const
{
        return m_sVal.GetPtr();
}

long CVarDefCont::GetValNum() const
{
        if ( m_fIsNum )
                return m_iVal;
        return StrToLong( m_sVal );
}

void CVarDefCont::SetValStr( LPCTSTR pszVal, bool fQuoted )
{
        m_sVal = pszVal ? pszVal : k_szEmpty;
        m_fQuoted = fQuoted;
        m_fIsNum = false;
        m_iVal = StrToLong( m_sVal );
}

void CVarDefCont::SetValNum( long iVal )
{
        m_iVal = iVal;
        m_fIsNum = true;
        m_fQuoted = false;
        m_sVal.FormatVal( iVal );
}

CVarDefCont * CVarDefCont::CopySelf() const
{
        CVarDefCont * pNew = new CVarDefCont( m_sKey );
        if ( m_fIsNum )
                pNew->SetValNum( m_iVal );
        else
                pNew->SetValStr( m_sVal, m_fQuoted );
        return pNew;
}

/***************************************************************************/
// CVarDefMap helpers
/***************************************************************************/

CGString CVarDefMap::NormalizeKey( LPCTSTR pszKey )
{
        CGString sKey( pszKey ? pszKey : k_szEmpty );
        ToLower( sKey );
        return sKey;
}

void CVarDefMap::ToLower( CGString & sKey )
{
        TCHAR * pBuffer = sKey.GetBuffer();
        if ( pBuffer == NULL )
                return;
        for ( size_t i = 0; pBuffer[i] != '\0'; ++i )
        {
                pBuffer[i] = static_cast<TCHAR>( tolower( static_cast<unsigned char>( pBuffer[i] )));
        }
}

bool CVarDefMap::TryParseNumber( LPCTSTR pszVal, long & lVal )
{
        if ( pszVal == NULL || *pszVal == '\0' )
                return false;
        TCHAR * pEnd = NULL;
        long lTest = strtol( pszVal, &pEnd, 0 );
        if ( pEnd == NULL )
                return false;
        if ( *pEnd != '\0' )
                return false;
        lVal = lTest;
        return true;
}

bool CVarDefMap::ShouldWriteQuoted( const CVarDefCont * pVar )
{
        if ( pVar == NULL )
                return false;
        if ( pVar->IsNum())
                return false;
        if ( pVar->IsQuoted())
                return true;
        const TCHAR * pszVal = pVar->GetValStr();
        if ( pszVal == NULL || *pszVal == '\0' )
                return true;
        for ( ; *pszVal != '\0'; ++pszVal )
        {
                if ( isspace( static_cast<unsigned char>(*pszVal) ))
                        return true;
        }
        return false;
}

CVarDefCont * CVarDefMap::Find( LPCTSTR pszKey ) const
{
        if ( pszKey == NULL || *pszKey == '\0' )
                return NULL;
        CGString sKey = NormalizeKey( pszKey );
        for ( EntryList::const_iterator it = m_Entries.begin(); it != m_Entries.end(); ++it )
        {
                CVarDefCont * pEntry = (*it);
                if ( pEntry != NULL && ! strcmpi( pEntry->GetKey(), sKey.GetPtr()))
                        return pEntry;
        }
        return NULL;
}

CVarDefCont * CVarDefMap::AddNew( const CGString & sKey )
{
        CVarDefCont * pNew = new CVarDefCont( sKey.GetPtr());
        if ( pNew == NULL )
                return NULL;
        m_Entries.push_back( pNew );
        return pNew;
}

void CVarDefMap::Empty()
{
        for ( EntryList::iterator it = m_Entries.begin(); it != m_Entries.end(); ++it )
        {
                delete (*it);
        }
        m_Entries.clear();
}

/***************************************************************************/
// CVarDefMap public
/***************************************************************************/

CVarDefMap::CVarDefMap()
{
}

CVarDefMap::~CVarDefMap()
{
        Empty();
}

void CVarDefMap::Copy( const CVarDefMap * pOther )
{
        if ( pOther == NULL || pOther == this )
                return;
        Empty();
        for ( EntryList::const_iterator it = pOther->m_Entries.begin(); it != pOther->m_Entries.end(); ++it )
        {
                CVarDefCont * pVar = (*it);
                if ( pVar == NULL )
                        continue;
                m_Entries.push_back( pVar->CopySelf());
        }
}

CVarDefMap & CVarDefMap::operator = ( const CVarDefMap & other )
{
        if ( this != &other )
        {
                Copy( &other );
        }
        return *this;
}

size_t CVarDefMap::GetCount() const
{
        return m_Entries.size();
}

CVarDefCont * CVarDefMap::GetAt( size_t index ) const
{
        if ( index >= m_Entries.size())
                return NULL;
        return m_Entries[index];
}

CVarDefCont * CVarDefMap::GetKey( LPCTSTR pszKey ) const
{
        return Find( pszKey );
}

long CVarDefMap::GetKeyNum( LPCTSTR pszKey ) const
{
        CVarDefCont * pVar = Find( pszKey );
        return pVar ? pVar->GetValNum() : 0;
}

LPCTSTR CVarDefMap::GetKeyStr( LPCTSTR pszKey, bool fZero ) const
{
        CVarDefCont * pVar = Find( pszKey );
        if ( pVar == NULL )
                return fZero ? k_szZero : k_szEmpty;
        return pVar->GetValStr();
}

CVarDefCont * CVarDefMap::SetNum( LPCTSTR pszKey, long lVal, bool fZero )
{
        if ( pszKey == NULL || *pszKey == '\0' )
                return NULL;
        if ( fZero && lVal == 0 )
        {
                DeleteKey( pszKey );
                return NULL;
        }
        CVarDefCont * pVar = Find( pszKey );
        if ( pVar == NULL )
        {
                CGString sKey = NormalizeKey( pszKey );
                pVar = AddNew( sKey );
        }
        if ( pVar != NULL )
        {
                pVar->SetValNum( lVal );
        }
        return pVar;
}

CVarDefCont * CVarDefMap::SetStr( LPCTSTR pszKey, bool fQuoted, LPCTSTR pszVal, bool fZero )
{
        if ( pszKey == NULL || *pszKey == '\0' )
                return NULL;
        if ( pszVal == NULL || *pszVal == '\0' )
        {
                DeleteKey( pszKey );
                return NULL;
        }
        long lVal = 0;
        if ( !fQuoted && TryParseNumber( pszVal, lVal ))
                return SetNum( pszKey, lVal, fZero );
        CVarDefCont * pVar = Find( pszKey );
        if ( pVar == NULL )
        {
                CGString sKey = NormalizeKey( pszKey );
                pVar = AddNew( sKey );
        }
        if ( pVar != NULL )
        {
                pVar->SetValStr( pszVal, fQuoted );
        }
        return pVar;
}

void CVarDefMap::DeleteKey( LPCTSTR pszKey )
{
        if ( pszKey == NULL || *pszKey == '\0' )
                return;
        CGString sKey = NormalizeKey( pszKey );
        for ( EntryList::iterator it = m_Entries.begin(); it != m_Entries.end(); )
        {
                CVarDefCont * pVar = (*it);
                if ( pVar != NULL && ! strcmpi( pVar->GetKey(), sKey.GetPtr()))
                {
                        delete pVar;
                        it = m_Entries.erase( it );
                        return;
                }
                else
                        ++it;
        }
}

void CVarDefMap::ClearKeys( LPCTSTR pszMask )
{
        if ( pszMask == NULL || *pszMask == '\0' )
        {
                Empty();
                return;
        }
        CGString sMask = NormalizeKey( pszMask );
        for ( EntryList::iterator it = m_Entries.begin(); it != m_Entries.end(); )
        {
                CVarDefCont * pVar = (*it);
                if ( pVar != NULL && strstr( pVar->GetKey(), sMask.GetPtr()) != NULL )
                {
                        delete pVar;
                        it = m_Entries.erase( it );
                }
                else
                {
                        ++it;
                }
        }
}

void CVarDefMap::DumpKeys( CTextConsole * pSrc, LPCTSTR pszPrefix ) const
{
        if ( pSrc == NULL )
                return;
        LPCTSTR pszPre = ( pszPrefix != NULL ) ? pszPrefix : k_szEmpty;
        for ( EntryList::const_iterator it = m_Entries.begin(); it != m_Entries.end(); ++it )
        {
                const CVarDefCont * pVar = (*it);
                if ( pVar == NULL )
                        continue;
                pSrc->SysMessagef( "%s%s=%s", pszPre, pVar->GetKey(), pVar->GetValStr());
        }
}

bool CVarDefMap::r_LoadVal( CScript & s )
{
        bool fQuoted = false;
        return ( SetStr( s.GetKey(), fQuoted, s.GetArgStr( &fQuoted )) != NULL );
}

void CVarDefMap::r_WritePrefix( CScript & s, LPCTSTR pszPrefix, LPCTSTR pszExclude )
{
        CGString sKeyFull;
        bool fHasPrefix = ( pszPrefix != NULL && *pszPrefix != '\0' );
        for ( EntryList::const_iterator it = m_Entries.begin(); it != m_Entries.end(); ++it )
        {
                const CVarDefCont * pVar = (*it);
                if ( pVar == NULL )
                        continue;
                if ( pszExclude != NULL && *pszExclude != '\0' )
                {
                        if ( ! strcmpi( pszExclude, pVar->GetKey()))
                                continue;
                }
                if ( fHasPrefix )
                        sKeyFull.Format( "%s.%s", pszPrefix, pVar->GetKey());
                else
                        sKeyFull = pVar->GetKey();
                const TCHAR * pszVal = pVar->GetValStr();
                if ( ShouldWriteQuoted( pVar ))
                        s.WriteKeyFormat( sKeyFull, "\"%s\"", pszVal );
                else
                        s.WriteKey( sKeyFull, pszVal );
        }
}
