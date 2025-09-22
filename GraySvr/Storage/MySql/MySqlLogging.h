#pragma once

#if defined(UNIT_TEST)
#include "../../../tests/stubs/common_stub.h"
#else
#include "../../../Common/common.h"
#endif
#include "../Database.h"

WORD GetMySQLErrorLogMask( LOGL_TYPE level );
void LogDatabaseError( const Storage::DatabaseError & ex, LOGL_TYPE level );

