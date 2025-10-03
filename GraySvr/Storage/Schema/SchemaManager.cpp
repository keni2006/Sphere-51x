#ifndef UNIT_TEST
#include "../../graysvr.h"
#else
#include "../../../tests/stubs/graysvr.h"
#endif
#include "SchemaManager.h"

#include "../../MySqlStorageService.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{
        static const int SCHEMA_VERSION_ROW = 1;
        static const int SCHEMA_IMPORT_ROW = 2;
        static const int SCHEMA_WORLD_SAVECOUNT_ROW = 3;
        static const int SCHEMA_WORLD_SAVEFLAG_ROW = 4;
        static const int CURRENT_SCHEMA_VERSION = 4;
}

namespace Storage
{
namespace Schema
{

bool SchemaManager::EnsureSchemaVersionTable( MySqlStorageService & storage )
{
        const CGString sTableName = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery = storage.BuildSchemaVersionCreateQuery();
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
                (const char *) sTableName, SCHEMA_VERSION_ROW );
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
                (const char *) sTableName, SCHEMA_IMPORT_ROW );
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        return true;
}

int SchemaManager::GetSchemaVersion( MySqlStorageService & storage )
{
        const CGString sTableName = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;",
                (const char *) sTableName, SCHEMA_VERSION_ROW );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! storage.Query( sQuery, &result ))
        {
                return -1;
        }

        int iVersion = 0;
        if ( result && result->IsValid())
        {
                Storage::IDatabaseResult::Row pRow = result->FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        iVersion = atoi( pRow[0] );
                }
        }

        return iVersion;
}

bool SchemaManager::SetSchemaVersion( MySqlStorageService & storage, int version )
{
        const CGString sTableName = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "UPDATE `%s` SET `version` = %d WHERE `id` = %d;",
                (const char *) sTableName, version, SCHEMA_VERSION_ROW );
        return storage.ExecuteQuery( sQuery );
}

bool SchemaManager::ApplyMigration_0_1( MySqlStorageService & storage )
{
        const CGString sAccounts = storage.GetPrefixedTableName( "accounts" );
        const CGString sAccountEmails = storage.GetPrefixedTableName( "account_emails" );
        const CGString sCharacters = storage.GetPrefixedTableName( "characters" );
        const CGString sItems = storage.GetPrefixedTableName( "items" );
        const CGString sItemProps = storage.GetPrefixedTableName( "item_props" );
        const CGString sSectors = storage.GetPrefixedTableName( "sectors" );
        const CGString sGMPages = storage.GetPrefixedTableName( "gm_pages" );
        const CGString sServers = storage.GetPrefixedTableName( "servers" );
        const CGString sTimers = storage.GetPrefixedTableName( "timers" );

        std::vector<CGString> vQueries;
        CGString sQuery;
        CGString sCollationSuffix = storage.GetDefaultTableCollationSuffix();

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`name` VARCHAR(32) NOT NULL,"
                "`password` VARCHAR(64) NOT NULL,"
                "`plevel` INT NOT NULL DEFAULT 0,"
                "`priv_flags` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`status` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`comment` TEXT NULL,"
                "`email` VARCHAR(128) NULL,"
                "`chat_name` VARCHAR(64) NULL,"
                "`language` CHAR(3) NULL,"
                "`total_connect_time` INT NOT NULL DEFAULT 0,"
                "`last_connect_time` INT NOT NULL DEFAULT 0,"
                "`last_ip` VARCHAR(45) NULL,"
                "`last_login` DATETIME NULL,"
                "`first_ip` VARCHAR(45) NULL,"
                "`first_login` DATETIME NULL,"
                "`last_char_uid` BIGINT UNSIGNED NULL,"
                "`email_failures` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_accounts_name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sAccounts,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`account_id` INT UNSIGNED NOT NULL,"
                "`sequence` SMALLINT UNSIGNED NOT NULL,"
                "`message_id` SMALLINT UNSIGNED NOT NULL,"
                "PRIMARY KEY (`account_id`, `sequence`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sAccountEmails, (const char *) sAccounts, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`uid` BIGINT UNSIGNED NOT NULL,"
                "`account_id` INT UNSIGNED NULL,"
                "`name` VARCHAR(64) NULL,"
                "`body_id` INT NULL,"
                "`position_x` INT NULL,"
                "`position_y` INT NULL,"
                "`position_z` INT NULL,"
                "`map_plane` INT NULL,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`uid`),"
                "KEY `ix_characters_account` (`account_id`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sCharacters, (const char *) sAccounts, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`uid` BIGINT UNSIGNED NOT NULL,"
                "`container_uid` BIGINT UNSIGNED NULL,"
                "`owner_uid` BIGINT UNSIGNED NULL,"
                "`type` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`amount` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`color` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`position_x` INT NULL,"
                "`position_y` INT NULL,"
                "`position_z` INT NULL,"
                "`map_plane` INT NULL,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`uid`),"
                "KEY `ix_items_container` (`container_uid`),"
                "KEY `ix_items_owner` (`owner_uid`),"
                "FOREIGN KEY (`container_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,"
                "FOREIGN KEY (`owner_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sItems, (const char *) sItems, (const char *) sCharacters, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`item_uid` BIGINT UNSIGNED NOT NULL,"
                "`prop` VARCHAR(64) NOT NULL,"
                "`value` TEXT NULL,"
                "PRIMARY KEY (`item_uid`, `prop`),"
                "FOREIGN KEY (`item_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sItemProps, (const char *) sItems, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`map_plane` INT NOT NULL,"
                "`x1` INT NOT NULL,"
                "`y1` INT NOT NULL,"
                "`x2` INT NOT NULL,"
                "`y2` INT NOT NULL,"
                "`last_update` DATETIME NULL,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_sectors_bounds` (`map_plane`, `x1`, `y1`, `x2`, `y2`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sSectors, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`account_id` INT UNSIGNED NULL,"
                "`character_uid` BIGINT UNSIGNED NULL,"
                "`reason` TEXT NULL,"
                "`status` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`id`),"
                "KEY `ix_gm_pages_account` (`account_id`),"
                "KEY `ix_gm_pages_character` (`character_uid`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL,"
                "FOREIGN KEY (`character_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sGMPages, (const char *) sAccounts, (const char *) sCharacters, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`name` VARCHAR(64) NOT NULL,"
                "`address` VARCHAR(128) NULL,"
                "`port` INT NULL,"
                "`status` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`last_seen` DATETIME NULL,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_servers_name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sServers, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`character_uid` BIGINT UNSIGNED NULL,"
                "`item_uid` BIGINT UNSIGNED NULL,"
                "`expires_at` BIGINT NOT NULL,"
                "`type` INT UNSIGNED NOT NULL,"
                "`data` TEXT NULL,"
                "PRIMARY KEY (`id`),"
                "KEY `ix_timers_character` (`character_uid`),"
                "KEY `ix_timers_item` (`item_uid`),"
                "FOREIGN KEY (`character_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,"
                "FOREIGN KEY (`item_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sTimers, (const char *) sCharacters, (const char *) sItems, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        for ( size_t i = 0; i < vQueries.size(); ++i )
        {
                if ( ! storage.ExecuteQuery( vQueries[i] ))
                {
                        return false;
                }
        }

        return true;
}

bool SchemaManager::ApplyMigration_1_2( MySqlStorageService & storage )
{
        const CGString sAccounts = storage.GetPrefixedTableName( "accounts" );
        const CGString sAccountEmails = storage.GetPrefixedTableName( "account_emails" );
        const CGString sTableName = storage.GetPrefixedTableName( "schema_version" );

        if ( ! EnsureColumnExists( storage, sAccounts, "priv_flags", "`priv_flags` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `plevel`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "comment", "`comment` TEXT NULL AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "chat_name", "`chat_name` VARCHAR(64) NULL AFTER `email`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "language", "`language` CHAR(3) NULL AFTER `chat_name`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "total_connect_time", "`total_connect_time` INT NOT NULL DEFAULT 0 AFTER `language`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "last_connect_time", "`last_connect_time` INT NOT NULL DEFAULT 0 AFTER `total_connect_time`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "first_ip", "`first_ip` VARCHAR(45) NULL AFTER `last_ip`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "first_login", "`first_login` DATETIME NULL AFTER `first_ip`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "last_char_uid", "`last_char_uid` BIGINT UNSIGNED NULL AFTER `first_login`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "email_failures", "`email_failures` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `last_char_uid`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sAccounts, "updated_at", "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP AFTER `created_at`" ))
        {
                return false;
        }

        CGString sQuery;
        CGString sCollationSuffix = storage.GetDefaultTableCollationSuffix();
        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`account_id` INT UNSIGNED NOT NULL,"
                "`sequence` SMALLINT UNSIGNED NOT NULL,"
                "`message_id` SMALLINT UNSIGNED NOT NULL,"
                "PRIMARY KEY (`account_id`, `sequence`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sAccountEmails, (const char *) sAccounts, storage.GetDefaultTableCharset(), (const char *) sCollationSuffix );
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
                (const char *) sTableName, SCHEMA_WORLD_SAVECOUNT_ROW );
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
                (const char *) sTableName, SCHEMA_WORLD_SAVEFLAG_ROW );
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        return true;
}

bool SchemaManager::ApplyMigration_2_3( MySqlStorageService & storage )
{
        const CGString sAccounts = storage.GetPrefixedTableName( "accounts" );
        const CGString sWorldObjects = storage.GetPrefixedTableName( "world_objects" );
        const CGString sWorldObjectData = storage.GetPrefixedTableName( "world_object_data" );
        const CGString sWorldObjectComponents = storage.GetPrefixedTableName( "world_object_components" );
        const CGString sWorldObjectRelations = storage.GetPrefixedTableName( "world_object_relations" );
        const CGString sWorldSavepoints = storage.GetPrefixedTableName( "world_savepoints" );
        const CGString sWorldObjectAudit = storage.GetPrefixedTableName( "world_object_audit" );

        std::vector<CGString> vQueries;
        CGString sQuery;
        CGString sCollationSuffix = storage.GetDefaultTableCollationSuffix();

        const bool fRelationsTableExists = ColumnExists( storage, sWorldObjectRelations, "parent_uid" );
        if ( fRelationsTableExists )
        {
                bool fRelationColumnExists = ColumnExists( storage, sWorldObjectRelations, "relation" );
                if ( ! fRelationColumnExists && ColumnExists( storage, sWorldObjectRelations, "type" ))
                {
                        CGString sRenameQuery;
                        sRenameQuery.Format(
                                "ALTER TABLE `%s` CHANGE COLUMN `type` `relation` VARCHAR(32) NOT NULL;",
                                (const char *) sWorldObjectRelations );
                        if ( ! storage.ExecuteQuery( sRenameQuery ))
                        {
                                return false;
                        }

                        fRelationColumnExists = true;
                }

                if ( ! fRelationColumnExists )
                {
                        CGString sAddRelation;
                        sAddRelation.Format(
                                "ALTER TABLE `%s` ADD COLUMN `relation` VARCHAR(32) NOT NULL DEFAULT 'container' AFTER `child_uid`;",
                                (const char *) sWorldObjectRelations );
                        if ( ! storage.ExecuteQuery( sAddRelation ))
                        {
                                return false;
                        }

                        CGString sPopulateRelation;
                        sPopulateRelation.Format(
                                "UPDATE `%s` SET `relation` = 'container' WHERE `relation` IS NULL OR `relation` = '';",
                                (const char *) sWorldObjectRelations );
                        if ( ! storage.ExecuteQuery( sPopulateRelation ))
                        {
                                return false;
                        }
                }

                if ( ! ColumnExists( storage, sWorldObjectRelations, "sequence" ))
                {
                        if ( ! EnsureColumnExists( storage, sWorldObjectRelations, "sequence",
                                "`sequence` INT NOT NULL DEFAULT 0 AFTER `relation`" ))
                        {
                                return false;
                        }
                }
        }

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`uid` BIGINT UNSIGNED NOT NULL,\n"
                "`object_type` VARCHAR(32) NOT NULL,\n"
                "`object_subtype` VARCHAR(64) NULL,\n"
                "`name` VARCHAR(128) NULL,\n"
                "`account_id` INT UNSIGNED NULL,\n"
                "`container_uid` BIGINT UNSIGNED NULL,\n"
                "`top_level_uid` BIGINT UNSIGNED NULL,\n"
                "`position_x` INT NULL,\n"
                "`position_y` INT NULL,\n"
                "`position_z` INT NULL,\n"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,\n"
                "PRIMARY KEY (`uid`),\n"
                "KEY `ix_world_objects_type` (`object_type`),\n"
                "KEY `ix_world_objects_account` (`account_id`),\n"
                "KEY `ix_world_objects_container` (`container_uid`),\n"
                "KEY `ix_world_objects_top` (`top_level_uid`),\n"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL,\n"
                "FOREIGN KEY (`container_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL,\n"
                "FOREIGN KEY (`top_level_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjects,
                (const char *) sAccounts,
                (const char *) sWorldObjects,
                (const char *) sWorldObjects,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`object_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`data` LONGTEXT NOT NULL,\n"
                "`checksum` VARCHAR(64) NULL,\n"
                "PRIMARY KEY (`object_uid`)\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectData,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`object_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`component` VARCHAR(64) NOT NULL,\n"
                "`name` VARCHAR(64) NOT NULL,\n"
                "`sequence` INT NOT NULL,\n"
                "`value` TEXT NULL,\n"
                "PRIMARY KEY (`object_uid`, `component`, `name`),\n"
                "FOREIGN KEY (`object_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectComponents,
                (const char *) sWorldObjects,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`parent_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`child_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`relation` VARCHAR(32) NOT NULL,\n"
                "`sequence` INT NOT NULL DEFAULT 0,\n"
                "PRIMARY KEY (`parent_uid`, `child_uid`, `relation`, `sequence`),\n"
                "KEY `ix_world_relations_child` (`child_uid`),\n"
                "FOREIGN KEY (`parent_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,\n"
                "FOREIGN KEY (`child_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectRelations,
                (const char *) sWorldObjects,
                (const char *) sWorldObjects,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,\n"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`save_reason` VARCHAR(64) NULL,\n"
                "`checksum` VARCHAR(64) NULL,\n"
                "PRIMARY KEY (`id`)\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldSavepoints,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,\n"
                "`object_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`changed_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`change_type` VARCHAR(32) NOT NULL,\n"
                "`data_before` LONGTEXT NULL,\n"
                "`data_after` LONGTEXT NULL,\n"
                "PRIMARY KEY (`id`),\n"
                "KEY `ix_world_audit_object` (`object_uid`),\n"
                "FOREIGN KEY (`object_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectAudit,
                (const char *) sWorldObjects,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        vQueries.push_back( sQuery );

        for ( size_t i = 0; i < vQueries.size(); ++i )
        {
                if ( ! storage.ExecuteQuery( vQueries[i] ))
                {
                        return false;
                }
        }

        return true;
}

bool SchemaManager::ApplyMigration_3_4( MySqlStorageService & storage )
{
        const CGString sWorldSavepoints = storage.GetPrefixedTableName( "world_savepoints" );

        CGString sQuery;
        CGString sCollationSuffix = storage.GetDefaultTableCollationSuffix();
        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,\n"
                "`label` VARCHAR(64) NULL,\n"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`objects_count` INT NOT NULL DEFAULT 0,\n"
                "`checksum` VARCHAR(64) NULL,\n"
                "PRIMARY KEY (`id`)\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldSavepoints,
                storage.GetDefaultTableCharset(),
                (const char *) sCollationSuffix );
        if ( ! storage.ExecuteQuery( sQuery ))
        {
                return false;
        }

        if ( ColumnExists( storage, sWorldSavepoints, "save_reason" ) && ! ColumnExists( storage, sWorldSavepoints, "label" ))
        {
                CGString sRenameQuery;
                sRenameQuery.Format(
                        "ALTER TABLE `%s` CHANGE COLUMN `save_reason` `label` VARCHAR(64) NULL;",
                        (const char *) sWorldSavepoints );
                if ( ! storage.ExecuteQuery( sRenameQuery ))
                {
                        return false;
                }
        }

        if ( ! EnsureColumnExists( storage, sWorldSavepoints, "label",
                "`label` VARCHAR(64) NULL AFTER `id`" ))
        {
                return false;
        }

        if ( ! EnsureColumnExists( storage, sWorldSavepoints, "objects_count",
                "`objects_count` INT NOT NULL DEFAULT 0 AFTER `created_at`" ))
        {
                return false;
        }

        return true;
}

bool SchemaManager::EnsureColumnExists( MySqlStorageService & storage, const CGString & table, const char * column, const char * definition )
{
        if ( ColumnExists( storage, table, column ))
        {
                return true;
        }

        CGString sQuery;
        sQuery.Format( "ALTER TABLE `%s` ADD COLUMN %s;", (const char *) table, definition );
        return storage.ExecuteQuery( sQuery );
}

bool SchemaManager::ColumnExists( MySqlStorageService & storage, const CGString & table, const char * column ) const
{
        if ( ! storage.IsConnected())
        {
                return false;
        }
        if ( storage.m_sDatabaseName.IsEmpty())
        {
                return false;
        }

        CGString sEscDatabase = storage.EscapeString( (const TCHAR *) storage.m_sDatabaseName );
        CGString sEscTable = storage.EscapeString( (const TCHAR *) table );
        CGString sEscColumn = storage.EscapeString( column );

        CGString sQuery;
        sQuery.Format(
                "SELECT COUNT(*) FROM information_schema.columns "
                "WHERE table_schema = '%s' AND table_name = '%s' AND column_name = '%s';",
                (const char *) sEscDatabase, (const char *) sEscTable, (const char *) sEscColumn );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! storage.Query( sQuery, &result ))
        {
                return false;
        }

        bool fExists = false;
        if ( result && result->IsValid())
        {
                Storage::IDatabaseResult::Row pRow = result->FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        fExists = ( atoi( pRow[0] ) > 0 );
                }
        }

        return fExists;
}

bool SchemaManager::InsertOrUpdateSchemaValue( MySqlStorageService & storage, int id, int value )
{
        const CGString sTableName = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format(
                "INSERT INTO `%s` (`id`, `version`) VALUES (%d, %d) ON DUPLICATE KEY UPDATE `version` = VALUES(`version`);",
                (const char *) sTableName, id, value );
        return storage.ExecuteQuery( sQuery );
}

bool SchemaManager::QuerySchemaValue( MySqlStorageService & storage, int id, int & value )
{
        value = 0;
        const CGString sTableName = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;",
                (const char *) sTableName, id );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! storage.Query( sQuery, &result ))
        {
                return false;
        }

        if ( result && result->IsValid())
        {
                Storage::IDatabaseResult::Row pRow = result->FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        value = atoi( pRow[0] );
                }
        }

        return true;
}

bool SchemaManager::EnsureSectorColumns( MySqlStorageService & storage )
{
        static bool s_fEnsured = false;
        if ( s_fEnsured )
        {
                return true;
        }

        const CGString sSectors = storage.GetPrefixedTableName( "sectors" );
        if ( ! EnsureColumnExists( storage, sSectors, "has_light_override", "`has_light_override` TINYINT(1) NOT NULL DEFAULT 0 AFTER `last_update`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sSectors, "local_light", "`local_light` INT NULL AFTER `has_light_override`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sSectors, "has_rain_override", "`has_rain_override` TINYINT(1) NOT NULL DEFAULT 0 AFTER `local_light`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sSectors, "rain_chance", "`rain_chance` INT NULL AFTER `has_rain_override`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sSectors, "has_cold_override", "`has_cold_override` TINYINT(1) NOT NULL DEFAULT 0 AFTER `rain_chance`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sSectors, "cold_chance", "`cold_chance` INT NULL AFTER `has_cold_override`" ))
        {
                return false;
        }

        s_fEnsured = true;
        return true;
}

bool SchemaManager::EnsureGMPageColumns( MySqlStorageService & storage )
{
        static bool s_fEnsured = false;
        if ( s_fEnsured )
        {
                return true;
        }

        const CGString sGMPages = storage.GetPrefixedTableName( "gm_pages" );
        if ( ! EnsureColumnExists( storage, sGMPages, "account_name", "`account_name` VARCHAR(32) NULL AFTER `account_id`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sGMPages, "page_time", "`page_time` BIGINT NULL AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sGMPages, "pos_x", "`pos_x` INT NULL AFTER `page_time`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sGMPages, "pos_y", "`pos_y` INT NULL AFTER `pos_x`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sGMPages, "pos_z", "`pos_z` INT NULL AFTER `pos_y`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sGMPages, "map_plane", "`map_plane` INT NULL AFTER `pos_z`" ))
        {
                return false;
        }

        s_fEnsured = true;
        return true;
}

bool SchemaManager::EnsureServerColumns( MySqlStorageService & storage )
{
        static bool s_fEnsured = false;
        if ( s_fEnsured )
        {
                return true;
        }

        const CGString sServers = storage.GetPrefixedTableName( "servers" );
        if ( ! EnsureColumnExists( storage, sServers, "time_zone", "`time_zone` INT NOT NULL DEFAULT 0 AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "clients_avg", "`clients_avg` INT NOT NULL DEFAULT 0 AFTER `time_zone`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "url", "`url` VARCHAR(255) NULL AFTER `clients_avg`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "email", "`email` VARCHAR(128) NULL AFTER `url`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "register_password", "`register_password` VARCHAR(64) NULL AFTER `email`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "notes", "`notes` TEXT NULL AFTER `register_password`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "language", "`language` VARCHAR(16) NULL AFTER `notes`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "version", "`version` VARCHAR(64) NULL AFTER `language`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "acc_app", "`acc_app` INT NOT NULL DEFAULT 0 AFTER `version`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "last_valid_seconds", "`last_valid_seconds` INT NULL AFTER `acc_app`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( storage, sServers, "age_hours", "`age_hours` INT NULL AFTER `last_valid_seconds`" ))
        {
                return false;
        }

        s_fEnsured = true;
        return true;
}

bool SchemaManager::ApplyMigration( MySqlStorageService & storage, int fromVersion )
{
        switch ( fromVersion )
        {
        case 0:
                if ( ! ApplyMigration_0_1( storage ))
                {
                        return false;
                }
                if ( ! SetSchemaVersion( storage, 1 ))
                {
                        return false;
                }
                break;

        case 1:
                if ( ! ApplyMigration_1_2( storage ))
                {
                        return false;
                }
                if ( ! SetSchemaVersion( storage, 2 ))
                {
                        return false;
                }
                break;

        case 2:
                if ( ! ApplyMigration_2_3( storage ))
                {
                        return false;
                }
                if ( ! SetSchemaVersion( storage, 3 ))
                {
                        return false;
                }
                break;

        case 3:
                if ( ! ApplyMigration_3_4( storage ))
                {
                        return false;
                }
                if ( ! SetSchemaVersion( storage, 4 ))
                {
                        return false;
                }
                break;

        default:
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Unknown MySQL schema migration from version %d.\n", fromVersion );
                return false;
        }

        return true;
}

bool SchemaManager::EnsureSchema( MySqlStorageService & storage )
{
        if ( ! storage.IsConnected())
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Cannot ensure schema without an active MySQL connection.\n" );
                return false;
        }

        if ( ! EnsureSchemaVersionTable( storage ))
        {
                return false;
        }

        int iVersion = GetSchemaVersion( storage );
        if ( iVersion < 0 )
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to read MySQL schema version.\n" );
                return false;
        }

        while ( iVersion < CURRENT_SCHEMA_VERSION )
        {
                if ( ! ApplyMigration( storage, iVersion ))
                {
                        return false;
                }
                iVersion = GetSchemaVersion( storage );
                if ( iVersion < 0 )
                {
                        g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to read MySQL schema version after migration.\n" );
                        return false;
                }
        }

        return true;
}

bool SchemaManager::IsLegacyImportCompleted( MySqlStorageService & storage )
{
        const CGString sTable = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;", (const char *) sTable, SCHEMA_IMPORT_ROW );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! storage.Query( sQuery, &result ))
        {
                return false;
        }

        bool fCompleted = false;
        if ( result && result->IsValid())
        {
                Storage::IDatabaseResult::Row pRow = result->FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        fCompleted = ( atoi( pRow[0] ) != 0 );
                }
        }
        return fCompleted;
}

bool SchemaManager::SetLegacyImportCompleted( MySqlStorageService & storage )
{
        const CGString sTable = storage.GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "UPDATE `%s` SET `version` = 1 WHERE `id` = %d;", (const char *) sTable, SCHEMA_IMPORT_ROW );
        return storage.ExecuteQuery( sQuery );
}

bool SchemaManager::SetWorldSaveCount( MySqlStorageService & storage, int saveCount )
{
        return InsertOrUpdateSchemaValue( storage, SCHEMA_WORLD_SAVECOUNT_ROW, saveCount );
}

bool SchemaManager::GetWorldSaveCount( MySqlStorageService & storage, int & saveCount )
{
        if ( ! QuerySchemaValue( storage, SCHEMA_WORLD_SAVECOUNT_ROW, saveCount ))
        {
                return false;
        }
        return true;
}

bool SchemaManager::SetWorldSaveCompleted( MySqlStorageService & storage, bool fCompleted )
{
        return InsertOrUpdateSchemaValue( storage, SCHEMA_WORLD_SAVEFLAG_ROW, fCompleted ? 1 : 0 );
}

bool SchemaManager::GetWorldSaveCompleted( MySqlStorageService & storage, bool & fCompleted )
{
        int iValue = 0;
        if ( ! QuerySchemaValue( storage, SCHEMA_WORLD_SAVEFLAG_ROW, iValue ))
        {
                return false;
        }
        fCompleted = ( iValue != 0 );
        return true;
}

bool SchemaManager::LoadWorldMetadata( MySqlStorageService & storage, int & saveCount, bool & fCompleted )
{
        if ( ! GetWorldSaveCount( storage, saveCount ))
        {
                return false;
        }
        if ( ! GetWorldSaveCompleted( storage, fCompleted ))
        {
                return false;
        }
        return true;
}

} // namespace Schema
} // namespace Storage

