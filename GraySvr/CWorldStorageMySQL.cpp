#include "graysvr.h"
#include "CWorldStorageMySQL.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

namespace
{
	static const int SCHEMA_VERSION_ROW = 1;      // Stores current schema version
	static const int SCHEMA_IMPORT_ROW = 2;       // Tracks legacy import state
	static const int CURRENT_SCHEMA_VERSION = 1;
}

CWorldStorageMySQL::CWorldStorageMySQL()
{
	m_pConnection = NULL;
	m_fAutoReconnect = false;
	m_iReconnectTries = 0;
	m_iReconnectDelay = 0;
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
	m_fAutoReconnect = false;
	m_iReconnectTries = 0;
	m_iReconnectDelay = 0;
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
		"`status` INT NOT NULL DEFAULT 0,"
		"`email` VARCHAR(128) NULL,"
		"`last_ip` VARCHAR(45) NULL,"
		"`last_login` DATETIME NULL,"
		"`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
		"PRIMARY KEY (`id`),"
		"UNIQUE KEY `ux_accounts_name` (`name`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
		(const char *) sAccounts );
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

