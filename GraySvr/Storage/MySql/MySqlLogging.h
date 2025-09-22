#pragma once

#include "../../../Common/common.h"
#include "Storage/Database.h"

WORD GetMySQLErrorLogMask( LOGL_TYPE level );
void LogDatabaseError( const Storage::DatabaseError & ex, LOGL_TYPE level );

