#if defined(UNIT_TEST)
#include "../../../tests/stubs/graysvr.h"
#else
#include "../../graysvr.h"
#endif
#include "MySqlLogging.h"

WORD GetMySQLErrorLogMask( LOGL_TYPE level )
{
        WORD wMask = static_cast<WORD>( level );
        if ( g_Serv.IsLoading())
        {
                wMask |= LOGM_INIT;
        }
        return wMask;
}

void LogDatabaseError( const Storage::DatabaseError & ex, LOGL_TYPE level )
{
        g_Log.Event( GetMySQLErrorLogMask( level ), "MySQL %s error (%u): %s\n", ex.GetContext().c_str(), ex.GetCode(), ex.what());
}

