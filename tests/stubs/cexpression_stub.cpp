#include "graycom.h"

int CVarDefArray::FindValNum( int ) const
{
    return -1;
}

int CVarDefArray::FindValStr( const TCHAR * ) const
{
    return -1;
}

CExpression g_Exp;

int CExpression::GetSingle( const TCHAR * & pArgs, bool fHexDef )
{
    if ( pArgs == NULL )
    {
        return 0;
    }

    char * endPtr = nullptr;
    long value = strtol( pArgs, &endPtr, fHexDef ? 16 : 10 );
    if ( endPtr == pArgs )
    {
        return 0;
    }

    pArgs = endPtr;
    return static_cast<int>( value );
}

int CExpression::GetVal( const TCHAR * & pArgs, bool fHexDef )
{
    return GetSingle( pArgs, fHexDef );
}

bool CExpression::SetVarDef( const TCHAR *, const TCHAR * )
{
    return false;
}

