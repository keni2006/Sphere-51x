#include "graysvr.h"
#include "CWorldStorageMySQL.h"

#include <algorithm>
#include <chrono>
#include <thread>

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

void CWorldStorageMySQL::LogMySQLError( const char * context )
{
	if ( m_pConnection == NULL )
	{
		g_Log.Event( LOGM_INIT|LOGL_ERROR, "MySQL %s error: no active connection.\n", context );
		return;
	}

	g_Log.Event( LOGM_INIT|LOGL_ERROR, "MySQL %s error (%u): %s\n", context, mysql_errno( m_pConnection ), mysql_error( m_pConnection ));
}
