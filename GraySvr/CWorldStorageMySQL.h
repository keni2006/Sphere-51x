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

	const CGString & GetTablePrefix() const
	{
		return m_sTablePrefix;
	}

private:
	void LogMySQLError( const char * context );

	MYSQL * m_pConnection;
	CGString m_sTablePrefix;
	bool m_fAutoReconnect;
	int m_iReconnectTries;
	int m_iReconnectDelay;
};

#endif // _CWORLD_STORAGE_MYSQL_H_
