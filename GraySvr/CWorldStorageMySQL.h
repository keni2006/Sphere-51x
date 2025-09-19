#ifndef _CWORLD_STORAGE_MYSQL_H_
#define _CWORLD_STORAGE_MYSQL_H_

#include "../common/cstring.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <winsock/mysql.h>
#else
#include <mysql/mysql.h>
#endif


struct CServerMySQLConfig;

class CWorldStorageMySQL
{
public:
	CWorldStorageMySQL();
	~CWorldStorageMySQL();

	bool Connect( const CServerMySQLConfig & config );
	void Disconnect();
	bool IsConnected() const;
	MYSQL * GetHandle() const;

	bool EnsureSchema();
	int GetSchemaVersion();

	const CGString & GetTablePrefix() const
	{
		return m_sTablePrefix;
	}

private:
	bool Query( const CGString & query, MYSQL_RES ** ppResult = NULL );
	bool ExecuteQuery( const CGString & query );
	bool EnsureSchemaVersionTable();
	bool SetSchemaVersion( int version );
	bool ApplyMigration( int fromVersion );
	bool ApplyMigration_0_1();
	CGString GetPrefixedTableName( const char * name ) const;

	void LogMySQLError( const char * context );

	MYSQL * m_pConnection;
	CGString m_sTablePrefix;
	bool m_fAutoReconnect;
	int m_iReconnectTries;
	int m_iReconnectDelay;
};

#endif // _CWORLD_STORAGE_MYSQL_H_

