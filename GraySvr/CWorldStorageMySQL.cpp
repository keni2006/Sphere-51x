#include "graysvr.h"
#include "CWorldStorageMySQL.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{
	static const int SCHEMA_VERSION_ROW = 1;      // Stores current schema version
	static const int SCHEMA_IMPORT_ROW = 2;       // Tracks legacy import state
        static const int CURRENT_SCHEMA_VERSION = 2;
}

CWorldStorageMySQL::CWorldStorageMySQL()
{
        m_pConnection = NULL;
        m_fAutoReconnect = false;
        m_iReconnectTries = 0;
        m_iReconnectDelay = 0;
        m_sDatabaseName.Empty();
        m_tLastAccountSync = 0;
}

CWorldStorageMySQL::~CWorldStorageMySQL()
{
	Disconnect();
}

bool CWorldStorageMySQL::Connect( const CServerMySQLConfig & config )
{
	Disconnect();

	if ( ! config.m_fEnable )
	{
		return false;
	}

        m_sTablePrefix = config.m_sTablePrefix;
        m_fAutoReconnect = config.m_fAutoReconnect;
        m_iReconnectTries = config.m_iReconnectTries;
        m_iReconnectDelay = config.m_iReconnectDelay;
        m_sDatabaseName = config.m_sDatabase;
        m_tLastAccountSync = 0;

	const int iAttempts = std::max( m_fAutoReconnect ? m_iReconnectTries : 1, 1 );

	for ( int iAttempt = 0; iAttempt < iAttempts; ++iAttempt )
	{
		m_pConnection = mysql_init( NULL );
		if ( m_pConnection == NULL )
		{
			g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to initialize MySQL handle.\n" );
			return false;
		}

#ifdef MYSQL_OPT_RECONNECT
		bool fReconnect = m_fAutoReconnect != 0;
		mysql_options( m_pConnection, MYSQL_OPT_RECONNECT, &fReconnect );
#endif

		const char * pszHost = config.m_sHost.IsEmpty() ? NULL : (const char *) config.m_sHost;
		const char * pszUser = config.m_sUser.IsEmpty() ? NULL : (const char *) config.m_sUser;
		const char * pszPassword = config.m_sPassword.IsEmpty() ? NULL : (const char *) config.m_sPassword;
		const char * pszDatabase = config.m_sDatabase.IsEmpty() ? NULL : (const char *) config.m_sDatabase;
		unsigned int uiPort = ( config.m_iPort > 0 ) ? (unsigned int) config.m_iPort : 0;

		MYSQL * pResult = mysql_real_connect( m_pConnection, pszHost, pszUser, pszPassword, pszDatabase, uiPort, NULL, 0 );
		if ( pResult != NULL )
		{
			g_Log.Event( LOGM_INIT|LOGL_EVENT, "Connected to MySQL server %s:%u.\n", pszHost ? pszHost : "localhost", uiPort );
			return true;
		}

		LogMySQLError( "mysql_real_connect" );
		mysql_close( m_pConnection );
		m_pConnection = NULL;

		if ( ( iAttempt + 1 ) < iAttempts )
		{
			int iDelay = std::max( m_iReconnectDelay, 1 );
			g_Log.Event( LOGM_INIT|LOGL_WARN, "Retrying MySQL connection in %d second(s).\n", iDelay );
			std::this_thread::sleep_for( std::chrono::seconds( iDelay ));
		}
	}

	g_Log.Event( LOGM_INIT|LOGL_ERROR, "Unable to connect to MySQL server after %d attempt(s).\n", std::max( iAttempts, 1 ) );
	return false;
}

void CWorldStorageMySQL::Disconnect()
{
	if ( m_pConnection != NULL )
	{
		mysql_close( m_pConnection );
		m_pConnection = NULL;
	}

        m_sTablePrefix.Empty();
        m_sDatabaseName.Empty();
        m_fAutoReconnect = false;
        m_iReconnectTries = 0;
        m_iReconnectDelay = 0;
        m_tLastAccountSync = 0;
}

bool CWorldStorageMySQL::IsConnected() const
{
	return m_pConnection != NULL;
}

MYSQL * CWorldStorageMySQL::GetHandle() const
{
	return m_pConnection;
}

CGString CWorldStorageMySQL::GetPrefixedTableName( const char * name ) const
{
	CGString sName;
	sName.Format( "%s%s", (const char *) m_sTablePrefix, name );
	return sName;
}

bool CWorldStorageMySQL::Query( const CGString & query, MYSQL_RES ** ppResult )
{
	if ( m_pConnection == NULL )
	{
		g_Log.Event( LOGM_INIT|LOGL_ERROR, "MySQL query attempted without an active connection.\n" );
		return false;
	}

	if ( mysql_query( m_pConnection, query ) != 0 )
	{
		LogMySQLError( "mysql_query" );
		g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed query: %s\n", (const char *) query );
		return false;
	}

	if ( ppResult != NULL )
	{
		*ppResult = mysql_store_result( m_pConnection );
		if ( *ppResult == NULL && mysql_errno( m_pConnection ) != 0 )
		{
			LogMySQLError( "mysql_store_result" );
			g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to fetch result for query: %s\n", (const char *) query );
			return false;
		}
	}
	else if ( mysql_field_count( m_pConnection ) != 0 )
	{
		MYSQL_RES * pResult = mysql_store_result( m_pConnection );
		if ( pResult != NULL )
		{
			mysql_free_result( pResult );
		}
	}

	return true;
}

bool CWorldStorageMySQL::ExecuteQuery( const CGString & query )
{
	return Query( query, NULL );
}

bool CWorldStorageMySQL::EnsureSchemaVersionTable()
{
	const CGString sTableName = GetPrefixedTableName( "schema_version" );

	CGString sQuery;
	sQuery.Format(
		"CREATE TABLE IF NOT EXISTS `%s` ("
		"`id` INT NOT NULL,"
		"`version` INT NOT NULL,"
		"PRIMARY KEY (`id`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sTableName );
	if ( ! ExecuteQuery( sQuery ))
	{
		return false;
	}

	sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
		(const char *) sTableName, SCHEMA_VERSION_ROW );
	if ( ! ExecuteQuery( sQuery ))
	{
		return false;
	}

	sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
		(const char *) sTableName, SCHEMA_IMPORT_ROW );
	if ( ! ExecuteQuery( sQuery ))
	{
		return false;
	}

	return true;
}

int CWorldStorageMySQL::GetSchemaVersion()
{
	const CGString sTableName = GetPrefixedTableName( "schema_version" );

	CGString sQuery;
	sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;",
		(const char *) sTableName, SCHEMA_VERSION_ROW );

	MYSQL_RES * pResult = NULL;
	if ( ! Query( sQuery, &pResult ))
	{
		return -1;
	}

	int iVersion = 0;
	if ( pResult != NULL )
	{
		MYSQL_ROW pRow = mysql_fetch_row( pResult );
		if ( pRow != NULL && pRow[0] != NULL )
		{
			iVersion = atoi( pRow[0] );
		}
		mysql_free_result( pResult );
	}

	return iVersion;
}

bool CWorldStorageMySQL::SetSchemaVersion( int version )
{
	const CGString sTableName = GetPrefixedTableName( "schema_version" );

	CGString sQuery;
	sQuery.Format( "UPDATE `%s` SET `version` = %d WHERE `id` = %d;",
		(const char *) sTableName, version, SCHEMA_VERSION_ROW );
	return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::ApplyMigration_0_1()
{
        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        const CGString sAccountEmails = GetPrefixedTableName( "account_emails" );
        const CGString sCharacters = GetPrefixedTableName( "characters" );
	const CGString sItems = GetPrefixedTableName( "items" );
	const CGString sItemProps = GetPrefixedTableName( "item_props" );
	const CGString sSectors = GetPrefixedTableName( "sectors" );
	const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
	const CGString sServers = GetPrefixedTableName( "servers" );
	const CGString sTimers = GetPrefixedTableName( "timers" );

	std::vector<CGString> vQueries;
	CGString sQuery;

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`name` VARCHAR(32) NOT NULL,"
                "`password` VARCHAR(64) NOT NULL,"
                "`plevel` INT NOT NULL DEFAULT 0,"
                "`priv_flags` INT NOT NULL DEFAULT 0,"
                "`status` INT NOT NULL DEFAULT 0,"
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
                "`email_failures` INT NOT NULL DEFAULT 0,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_accounts_name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
                (const char *) sAccounts );
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`account_id` INT UNSIGNED NOT NULL,"
                "`sequence` INT NOT NULL,"
                "`message_id` INT NOT NULL,"
                "PRIMARY KEY (`account_id`, `sequence`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
                (const char *) sAccountEmails, (const char *) sAccounts );
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
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sCharacters, (const char *) sAccounts );
	vQueries.push_back( sQuery );

	sQuery.Format(
		"CREATE TABLE IF NOT EXISTS `%s` ("
		"`uid` BIGINT UNSIGNED NOT NULL,"
		"`container_uid` BIGINT UNSIGNED NULL,"
		"`owner_uid` BIGINT UNSIGNED NULL,"
		"`type` INT NOT NULL DEFAULT 0,"
		"`amount` INT NOT NULL DEFAULT 0,"
		"`color` INT NOT NULL DEFAULT 0,"
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
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sItems, (const char *) sItems, (const char *) sCharacters );
	vQueries.push_back( sQuery );

	sQuery.Format(
		"CREATE TABLE IF NOT EXISTS `%s` ("
		"`item_uid` BIGINT UNSIGNED NOT NULL,"
		"`prop` VARCHAR(64) NOT NULL,"
		"`value` TEXT NULL,"
		"PRIMARY KEY (`item_uid`, `prop`),"
		"FOREIGN KEY (`item_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sItemProps, (const char *) sItems );
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
		"UNIQUE KEY `ux_sectors_bounds` (`map_plane`, `x1`, `y1`, `x2`, `y2`)
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sSectors );
	vQueries.push_back( sQuery );

	sQuery.Format(
		"CREATE TABLE IF NOT EXISTS `%s` ("
		"`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
		"`account_id` INT UNSIGNED NULL,"
		"`character_uid` BIGINT UNSIGNED NULL,"
		"`reason` TEXT NULL,"
		"`status` INT NOT NULL DEFAULT 0,"
		"`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
		"PRIMARY KEY (`id`),"
		"KEY `ix_gm_pages_account` (`account_id`),"
		"KEY `ix_gm_pages_character` (`character_uid`),"
		"FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL,"
		"FOREIGN KEY (`character_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sGMPages, (const char *) sAccounts, (const char *) sCharacters );
	vQueries.push_back( sQuery );

	sQuery.Format(
		"CREATE TABLE IF NOT EXISTS `%s` ("
		"`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
		"`name` VARCHAR(64) NOT NULL,"
		"`address` VARCHAR(128) NULL,"
		"`port` INT NULL,"
		"`status` INT NOT NULL DEFAULT 0,"
		"`last_seen` DATETIME NULL,"
		"PRIMARY KEY (`id`),"
		"UNIQUE KEY `ux_servers_name` (`name`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sServers );
	vQueries.push_back( sQuery );

	sQuery.Format(
		"CREATE TABLE IF NOT EXISTS `%s` ("
		"`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
		"`character_uid` BIGINT UNSIGNED NULL,"
		"`item_uid` BIGINT UNSIGNED NULL,"
		"`expires_at` BIGINT NOT NULL,"
		"`type` INT NOT NULL,"
		"`data` TEXT NULL,"
		"PRIMARY KEY (`id`),"
		"KEY `ix_timers_character` (`character_uid`),"
		"KEY `ix_timers_item` (`item_uid`),"
		"FOREIGN KEY (`character_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,"
		"FOREIGN KEY (`item_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sTimers, (const char *) sCharacters, (const char *) sItems );
	vQueries.push_back( sQuery );

	for ( size_t i = 0; i < vQueries.size(); ++i )
	{
		if ( ! ExecuteQuery( vQueries[i] ))
		{
			return false;
		}
        }

        return true;
}

bool CWorldStorageMySQL::ApplyMigration_1_2()
{
        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        const CGString sAccountEmails = GetPrefixedTableName( "account_emails" );

        if ( ! EnsureColumnExists( sAccounts, "priv_flags", "`priv_flags` INT NOT NULL DEFAULT 0 AFTER `plevel`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "comment", "`comment` TEXT NULL AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "chat_name", "`chat_name` VARCHAR(64) NULL AFTER `email`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "language", "`language` CHAR(3) NULL AFTER `chat_name`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "total_connect_time", "`total_connect_time` INT NOT NULL DEFAULT 0 AFTER `language`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "last_connect_time", "`last_connect_time` INT NOT NULL DEFAULT 0 AFTER `total_connect_time`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "first_ip", "`first_ip` VARCHAR(45) NULL AFTER `last_ip`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "first_login", "`first_login` DATETIME NULL AFTER `first_ip`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "last_char_uid", "`last_char_uid` BIGINT UNSIGNED NULL AFTER `first_login`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "email_failures", "`email_failures` INT NOT NULL DEFAULT 0 AFTER `last_char_uid`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "updated_at", "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP AFTER `created_at`" ))
        {
                return false;
        }

        CGString sQuery;
        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`account_id` INT UNSIGNED NOT NULL,"
                "`sequence` INT NOT NULL,"
                "`message_id` INT NOT NULL,"
                "PRIMARY KEY (`account_id`, `sequence`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
                (const char *) sAccountEmails, (const char *) sAccounts );
        if ( ! ExecuteQuery( sQuery ))
        {
                return false;
        }

        return true;
}

bool CWorldStorageMySQL::ColumnExists( const CGString & table, const char * column ) const
{
        if ( m_pConnection == NULL )
        {
                return false;
        }
        if ( m_sDatabaseName.IsEmpty())
        {
                return false;
        }

        CGString sEscDatabase = EscapeString( (const TCHAR *) m_sDatabaseName );
        CGString sEscTable = EscapeString( (const TCHAR *) table );
        CGString sEscColumn = EscapeString( column );

        CGString sQuery;
        sQuery.Format(
                "SELECT COUNT(*) FROM information_schema.columns "
                "WHERE table_schema = '%s' AND table_name = '%s' AND column_name = '%s';",
                (const char *) sEscDatabase, (const char *) sEscTable, (const char *) sEscColumn );

        MYSQL_RES * pResult = NULL;
        if ( ! const_cast<CWorldStorageMySQL*>(this)->Query( sQuery, &pResult ))
        {
                return false;
        }

        bool fExists = false;
        if ( pResult != NULL )
        {
                MYSQL_ROW pRow = mysql_fetch_row( pResult );
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        fExists = ( atoi( pRow[0] ) > 0 );
                }
                mysql_free_result( pResult );
        }

        return fExists;
}

bool CWorldStorageMySQL::EnsureColumnExists( const CGString & table, const char * column, const char * definition )
{
        if ( ColumnExists( table, column ))
        {
                return true;
        }

        CGString sQuery;
        sQuery.Format( "ALTER TABLE `%s` ADD COLUMN %s;", (const char *) table, definition );
        return ExecuteQuery( sQuery );
}

CGString CWorldStorageMySQL::EscapeString( const TCHAR * pszInput ) const
{
        CGString sResult;
        if ( pszInput == NULL )
        {
                return sResult;
        }

        size_t uiLength = strlen( pszInput );
        if ( uiLength == 0 )
        {
                return sResult;
        }

        if ( m_pConnection == NULL )
        {
                sResult = pszInput;
                return sResult;
        }

        std::vector<char> buffer( uiLength * 2 + 1 );
        mysql_real_escape_string( m_pConnection, &buffer[0], pszInput, uiLength );
        sResult = &buffer[0];
        return sResult;
}

CGString CWorldStorageMySQL::FormatStringValue( const CGString & value ) const
{
        CGString sEscaped = EscapeString( (const TCHAR *) value );
        CGString sResult;
        sResult.Format( "'%s'", (const char *) sEscaped );
        return sResult;
}

CGString CWorldStorageMySQL::FormatOptionalStringValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString CWorldStorageMySQL::FormatDateTimeValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString CWorldStorageMySQL::FormatDateTimeValue( const CRealTime & value ) const
{
        if ( ! value.IsValid())
        {
                return CGString( "NULL" );
        }

        CGString sTemp;
        sTemp.Format( "%04d-%02d-%02d %02d:%02d:%02d",
                1900 + value.m_Year, value.m_Month + 1, value.m_Day,
                value.m_Hour, value.m_Min, value.m_Sec );
        return FormatStringValue( sTemp );
}

CGString CWorldStorageMySQL::FormatIPAddressValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString CWorldStorageMySQL::FormatIPAddressValue( const struct in_addr & value ) const
{
        if ( value.s_addr == 0 )
        {
                return CGString( "NULL" );
        }
        const char * pszIP = inet_ntoa( value );
        if ( pszIP == NULL )
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( CGString( pszIP ));
}

unsigned int CWorldStorageMySQL::GetAccountId( const CGString & name )
{
        if ( ! IsConnected())
        {
                return 0;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sEscName = EscapeString( (const TCHAR *) name );

        CGString sQuery;
        sQuery.Format( "SELECT `id` FROM `%s` WHERE `name` = '%s' LIMIT 1;", (const char *) sAccounts, (const char *) sEscName );

        MYSQL_RES * pResult = NULL;
        if ( ! Query( sQuery, &pResult ))
        {
                return 0;
        }

        unsigned int uiId = 0;
        if ( pResult != NULL )
        {
                MYSQL_ROW pRow = mysql_fetch_row( pResult );
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        uiId = (unsigned int) strtoul( pRow[0], NULL, 10 );
                }
                mysql_free_result( pResult );
        }
        return uiId;
}

bool CWorldStorageMySQL::FetchAccounts( std::vector<AccountData> & accounts, const CGString & whereClause )
{
        accounts.clear();

        if ( ! IsConnected())
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );

        CGString sQuery;
        sQuery.Format(
                "SELECT `id`,`name`,`password`,`plevel`,`priv_flags`,`status`,"
                "IFNULL(`comment`, ''),IFNULL(`email`, ''),IFNULL(`chat_name`, ''),"
                "IFNULL(`language`, ''),`total_connect_time`,`last_connect_time`,"
                "IFNULL(`last_ip`, ''),IFNULL(DATE_FORMAT(`last_login`, '%%Y-%%m-%%d %%H:%%i:%%s'), ''),"
                "IFNULL(`first_ip`, ''),IFNULL(DATE_FORMAT(`first_login`, '%%Y-%%m-%%d %%H:%%i:%%s'), ''),"
                "IFNULL(`last_char_uid`, 0),`email_failures`,UNIX_TIMESTAMP(`updated_at`)"
                " FROM `%s` %s;",
                (const char *) sAccounts,
                whereClause.IsEmpty() ? "" : (const char *) whereClause );

        MYSQL_RES * pResult = NULL;
        if ( ! Query( sQuery, &pResult ))
        {
                return false;
        }

        if ( pResult == NULL )
        {
            return true;
        }

        MYSQL_ROW pRow;
        while (( pRow = mysql_fetch_row( pResult )) != NULL )
        {
                AccountData data;
                data.m_id = pRow[0] ? (unsigned int) strtoul( pRow[0], NULL, 10 ) : 0;
                data.m_sName = pRow[1] ? pRow[1] : "";
                data.m_sPassword = pRow[2] ? pRow[2] : "";
                data.m_iPrivLevel = pRow[3] ? atoi( pRow[3] ) : 0;
                data.m_uPrivFlags = pRow[4] ? (unsigned int) strtoul( pRow[4], NULL, 10 ) : 0;
                data.m_uStatus = pRow[5] ? (unsigned int) strtoul( pRow[5], NULL, 10 ) : 0;
                data.m_sComment = pRow[6] ? pRow[6] : "";
                data.m_sEmail = pRow[7] ? pRow[7] : "";
                data.m_sChatName = pRow[8] ? pRow[8] : "";
                data.m_sLanguage = pRow[9] ? pRow[9] : "";
                data.m_iTotalConnectTime = pRow[10] ? atoi( pRow[10] ) : 0;
                data.m_iLastConnectTime = pRow[11] ? atoi( pRow[11] ) : 0;
                data.m_sLastIP = pRow[12] ? pRow[12] : "";
                data.m_sLastLogin = pRow[13] ? pRow[13] : "";
                data.m_sFirstIP = pRow[14] ? pRow[14] : "";
                data.m_sFirstLogin = pRow[15] ? pRow[15] : "";
#ifdef _WIN32
                data.m_uLastCharUID = pRow[16] ? (unsigned long long) _strtoui64( pRow[16], NULL, 10 ) : 0;
                data.m_tUpdatedAt = pRow[18] ? (time_t) _strtoi64( pRow[18], NULL, 10 ) : 0;
#else
                data.m_uLastCharUID = pRow[16] ? (unsigned long long) strtoull( pRow[16], NULL, 10 ) : 0;
                data.m_tUpdatedAt = pRow[18] ? (time_t) strtoll( pRow[18], NULL, 10 ) : 0;
#endif
                data.m_uEmailFailures = pRow[17] ? (unsigned int) strtoul( pRow[17], NULL, 10 ) : 0;
                data.m_EmailSchedule.clear();

                accounts.push_back( data );
        }

        mysql_free_result( pResult );
        return true;
}

void CWorldStorageMySQL::LoadAccountEmailSchedule( std::vector<AccountData> & accounts )
{
        if ( accounts.empty())
        {
                return;
        }

        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sQuery;
        sQuery.Format( "SELECT `account_id`,`sequence`,`message_id` FROM `%s` ORDER BY `account_id`,`sequence`;", (const char *) sEmails );

        MYSQL_RES * pResult = NULL;
        if ( ! Query( sQuery, &pResult ))
        {
                return;
        }

        if ( pResult == NULL )
        {
                return;
        }

        std::unordered_map<unsigned int, AccountData*> mapAccounts;
        for ( size_t i = 0; i < accounts.size(); ++i )
        {
                accounts[i].m_EmailSchedule.clear();
                mapAccounts[ accounts[i].m_id ] = &accounts[i];
        }

        MYSQL_ROW pRow;
        while (( pRow = mysql_fetch_row( pResult )) != NULL )
        {
                unsigned int accountId = pRow[0] ? (unsigned int) strtoul( pRow[0], NULL, 10 ) : 0;
                auto it = mapAccounts.find( accountId );
                if ( it == mapAccounts.end())
                {
                        continue;
                }

                unsigned int messageId = pRow[2] ? (unsigned int) strtoul( pRow[2], NULL, 10 ) : 0;
                it->second->m_EmailSchedule.push_back( (WORD) messageId );
        }

        mysql_free_result( pResult );
}

void CWorldStorageMySQL::UpdateAccountSyncTimestamp( const std::vector<AccountData> & accounts )
{
        time_t tMax = ( m_tLastAccountSync > 0 ) ? ( m_tLastAccountSync - 1 ) : 0;
        bool fUpdated = false;

        for ( size_t i = 0; i < accounts.size(); ++i )
        {
                if ( accounts[i].m_tUpdatedAt > tMax )
                {
                        tMax = accounts[i].m_tUpdatedAt;
                        fUpdated = true;
                }
        }

        if ( fUpdated )
        {
                m_tLastAccountSync = tMax + 1;
        }
        else if ( m_tLastAccountSync == 0 )
        {
                m_tLastAccountSync = time( NULL );
        }
}

bool CWorldStorageMySQL::LoadAllAccounts( std::vector<AccountData> & accounts )
{
        if ( ! FetchAccounts( accounts, CGString()))
        {
                return false;
        }
        LoadAccountEmailSchedule( accounts );
        UpdateAccountSyncTimestamp( accounts );
        return true;
}

bool CWorldStorageMySQL::LoadChangedAccounts( std::vector<AccountData> & accounts )
{
        if ( m_tLastAccountSync == 0 )
        {
                return LoadAllAccounts( accounts );
        }

        CGString sWhere;
        sWhere.Format( "WHERE `updated_at` >= FROM_UNIXTIME(%lld)", (long long) m_tLastAccountSync );
        if ( ! FetchAccounts( accounts, sWhere ))
        {
                return false;
        }
        LoadAccountEmailSchedule( accounts );
        UpdateAccountSyncTimestamp( accounts );
        return true;
}

bool CWorldStorageMySQL::UpsertAccount( const CAccount & account )
{
        if ( ! IsConnected())
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );

        CGString sNameValue = FormatStringValue( CGString( account.GetName()));
        CGString sPasswordValue = FormatStringValue( CGString( account.GetPassword()));
        CGString sCommentValue = FormatOptionalStringValue( account.m_sComment );
        CGString sEmailValue = FormatOptionalStringValue( account.m_sEMail );
        CGString sChatNameValue = FormatOptionalStringValue( account.m_sChatName );

        CGString sLanguageRaw;
        if ( account.m_lang[0] )
        {
                TCHAR szLang[4];
                szLang[0] = account.m_lang[0];
                szLang[1] = account.m_lang[1];
                szLang[2] = account.m_lang[2];
                szLang[3] = '\0';
                sLanguageRaw = szLang;
        }
        CGString sLanguageValue = FormatOptionalStringValue( sLanguageRaw );

        CGString sLastIPValue = FormatIPAddressValue( account.m_Last_IP );
        CGString sFirstIPValue = FormatIPAddressValue( account.m_First_IP );
        CGString sLastLoginValue = FormatDateTimeValue( account.m_Last_Connect_Date );
        CGString sFirstLoginValue = FormatDateTimeValue( account.m_First_Connect_Date );

        CGString sLastCharUIDValue;
        if ( account.m_uidLastChar.IsValidUID())
        {
                sLastCharUIDValue.Format( "%u", (unsigned int) account.m_uidLastChar );
        }
        else
        {
                sLastCharUIDValue = "NULL";
        }

        int statusValue = 0;
        if ( account.IsPriv( PRIV_BLOCKED ))
        {
                statusValue |= 0x1;
        }
        if ( account.IsPriv( PRIV_JAILED ))
        {
                statusValue |= 0x2;
        }

        CGString sQuery;
        sQuery.Format(
                "INSERT INTO `%s` ("
                "`name`,`password`,`plevel`,`priv_flags`,`status`,`comment`,`email`,`chat_name`,`language`,"
                "`total_connect_time`,`last_connect_time`,`last_ip`,`last_login`,`first_ip`,`first_login`,`last_char_uid`,`email_failures`)"
                " VALUES (%s,%s,%d,%u,%d,%s,%s,%s,%s,%d,%d,%s,%s,%s,%s,%s,%u)"
                " ON DUPLICATE KEY UPDATE "
                "`password`=VALUES(`password`),"
                "`plevel`=VALUES(`plevel`),"
                "`priv_flags`=VALUES(`priv_flags`),"
                "`status`=VALUES(`status`),"
                "`comment`=VALUES(`comment`),"
                "`email`=VALUES(`email`),"
                "`chat_name`=VALUES(`chat_name`),"
                "`language`=VALUES(`language`),"
                "`total_connect_time`=VALUES(`total_connect_time`),"
                "`last_connect_time`=VALUES(`last_connect_time`),"
                "`last_ip`=VALUES(`last_ip`),"
                "`last_login`=VALUES(`last_login`),"
                "`first_ip`=VALUES(`first_ip`),"
                "`first_login`=VALUES(`first_login`),"
                "`last_char_uid`=VALUES(`last_char_uid`),"
                "`email_failures`=VALUES(`email_failures`);",
                (const char *) sAccounts,
                (const char *) sNameValue,
                (const char *) sPasswordValue,
                account.GetPrivLevel(),
                (unsigned int) account.m_PrivFlags,
                statusValue,
                (const char *) sCommentValue,
                (const char *) sEmailValue,
                (const char *) sChatNameValue,
                (const char *) sLanguageValue,
                (int) account.m_Total_Connect_Time,
                (int) account.m_Last_Connect_Time,
                (const char *) sLastIPValue,
                (const char *) sLastLoginValue,
                (const char *) sFirstIPValue,
                (const char *) sFirstLoginValue,
                (const char *) sLastCharUIDValue,
                (unsigned int) account.m_iEmailFailures );

        if ( ! ExecuteQuery( sQuery ))
        {
                return false;
        }

        unsigned int accountId = GetAccountId( CGString( account.GetName()));
        if ( accountId == 0 )
        {
                return false;
        }

        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sDelete;
        sDelete.Format( "DELETE FROM `%s` WHERE `account_id` = %u;", (const char *) sEmails, accountId );
        if ( ! ExecuteQuery( sDelete ))
        {
                return false;
        }

        for ( int i = 0; i < account.m_EMailSchedule.GetCount(); ++i )
        {
                CGString sInsert;
                sInsert.Format( "INSERT INTO `%s` (`account_id`,`sequence`,`message_id`) VALUES (%u,%d,%u);",
                        (const char *) sEmails, accountId, i, (unsigned int) account.m_EMailSchedule[i] );
                if ( ! ExecuteQuery( sInsert ))
                {
                        return false;
                }
        }

        return true;
}

bool CWorldStorageMySQL::DeleteAccount( const TCHAR * pszAccountName )
{
        if ( ! IsConnected() || pszAccountName == NULL || pszAccountName[0] == '\0' )
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sEscName = EscapeString( pszAccountName );

        CGString sQuery;
        sQuery.Format( "DELETE FROM `%s` WHERE `name` = '%s';", (const char *) sAccounts, (const char *) sEscName );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::IsLegacyImportCompleted()
{
        const CGString sTable = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;", (const char *) sTable, SCHEMA_IMPORT_ROW );

        MYSQL_RES * pResult = NULL;
        if ( ! Query( sQuery, &pResult ))
        {
                return false;
        }

        bool fCompleted = false;
        if ( pResult != NULL )
        {
                MYSQL_ROW pRow = mysql_fetch_row( pResult );
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        fCompleted = ( atoi( pRow[0] ) != 0 );
                }
                mysql_free_result( pResult );
        }
        return fCompleted;
}

bool CWorldStorageMySQL::SetLegacyImportCompleted()
{
        const CGString sTable = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "UPDATE `%s` SET `version` = 1 WHERE `id` = %d;", (const char *) sTable, SCHEMA_IMPORT_ROW );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::ApplyMigration( int fromVersion )
{
        switch ( fromVersion )
        {
        case 0:
                if ( ! ApplyMigration_0_1())
                {
                        return false;
                }
                if ( ! SetSchemaVersion( 1 ))
                {
                        return false;
                }
                break;

        case 1:
                if ( ! ApplyMigration_1_2())
                {
                        return false;
                }
                if ( ! SetSchemaVersion( 2 ))
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

bool CWorldStorageMySQL::EnsureSchema()
{
	if ( ! IsConnected())
	{
		g_Log.Event( LOGM_INIT|LOGL_ERROR, "Cannot ensure schema without an active MySQL connection.\n" );
		return false;
	}

	if ( ! EnsureSchemaVersionTable())
	{
		return false;
	}

	int iVersion = GetSchemaVersion();
	if ( iVersion < 0 )
	{
		g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to read MySQL schema version.\n" );
		return false;
	}

	while ( iVersion < CURRENT_SCHEMA_VERSION )
	{
		if ( ! ApplyMigration( iVersion ))
		{
			return false;
		}
		iVersion = GetSchemaVersion();
		if ( iVersion < 0 )
		{
			g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to read MySQL schema version after migration.\n" );
			return false;
		}
	}

	return true;
}

void CWorldStorageMySQL::LogMySQLError( const char * context )
{
	if ( m_pConnection == NULL )
	{
		g_Log.Event( LOGM_INIT|LOGL_ERROR, "MySQL %s error: no active connection.\n", context );
		return;
	}

	g_Log.Event( LOGM_INIT|LOGL_ERROR, "MySQL %s error (%u): %s\n", context, mysql_errno( m_pConnection ), mysql_error( m_pConnection ));
}

