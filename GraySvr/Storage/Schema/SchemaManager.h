#pragma once

#include "../../../Common/cstring.h"

class MySqlStorageService;

namespace Storage
{
namespace Schema
{
        class SchemaManager
        {
        public:
                bool EnsureSchema( MySqlStorageService & storage );
                int GetSchemaVersion( MySqlStorageService & storage );
                bool IsLegacyImportCompleted( MySqlStorageService & storage );
                bool SetLegacyImportCompleted( MySqlStorageService & storage );
                bool SetWorldSaveCount( MySqlStorageService & storage, int saveCount );
                bool GetWorldSaveCount( MySqlStorageService & storage, int & saveCount );
                bool SetWorldSaveCompleted( MySqlStorageService & storage, bool fCompleted );
                bool GetWorldSaveCompleted( MySqlStorageService & storage, bool & fCompleted );
                bool LoadWorldMetadata( MySqlStorageService & storage, int & saveCount, bool & fCompleted );
                bool EnsureSectorColumns( MySqlStorageService & storage );
                bool EnsureGMPageColumns( MySqlStorageService & storage );
                bool EnsureServerColumns( MySqlStorageService & storage );

        private:
                friend class ::MySqlStorageService;

                bool EnsureSchemaVersionTable( MySqlStorageService & storage );
                bool SetSchemaVersion( MySqlStorageService & storage, int version );
                bool ApplyMigration( MySqlStorageService & storage, int fromVersion );
                bool ApplyMigration_0_1( MySqlStorageService & storage );
                bool ApplyMigration_1_2( MySqlStorageService & storage );
                bool ApplyMigration_2_3( MySqlStorageService & storage );
                bool EnsureColumnExists( MySqlStorageService & storage, const CGString & table, const char * column, const char * definition );
                bool ColumnExists( MySqlStorageService & storage, const CGString & table, const char * column ) const;
                bool InsertOrUpdateSchemaValue( MySqlStorageService & storage, int id, int value );
                bool QuerySchemaValue( MySqlStorageService & storage, int id, int & value );
        };
}
}

