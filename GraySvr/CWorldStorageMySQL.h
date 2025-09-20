#ifndef _CWORLD_STORAGE_MYSQL_H_
#define _CWORLD_STORAGE_MYSQL_H_

#include "../common/cstring.h"
#include <vector>

class CAccount;
class CRealTime;
struct in_addr;

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

        struct AccountData
        {
                unsigned int m_id;
                CGString m_sName;
                CGString m_sPassword;
                int m_iPrivLevel;
                unsigned int m_uPrivFlags;
                unsigned int m_uStatus;
                CGString m_sComment;
                CGString m_sEmail;
                CGString m_sChatName;
                CGString m_sLanguage;
                int m_iTotalConnectTime;
                int m_iLastConnectTime;
                CGString m_sLastIP;
                CGString m_sLastLogin;
                CGString m_sFirstIP;
                CGString m_sFirstLogin;
                unsigned long long m_uLastCharUID;
                unsigned int m_uEmailFailures;
                std::vector<WORD> m_EmailSchedule;
                time_t m_tUpdatedAt;
        };

        bool Connect( const CServerMySQLConfig & config );
        void Disconnect();
        bool IsConnected() const;
        MYSQL * GetHandle() const;

        bool EnsureSchema();
        int GetSchemaVersion();
        bool IsLegacyImportCompleted();
        bool SetLegacyImportCompleted();

        bool LoadAllAccounts( std::vector<AccountData> & accounts );
        bool LoadChangedAccounts( std::vector<AccountData> & accounts );
        bool UpsertAccount( const CAccount & account );
        bool DeleteAccount( const TCHAR * pszAccountName );

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
        bool ApplyMigration_1_2();
        bool EnsureColumnExists( const CGString & table, const char * column, const char * definition );
        bool ColumnExists( const CGString & table, const char * column ) const;
        bool FetchAccounts( std::vector<AccountData> & accounts, const CGString & whereClause );
        void LoadAccountEmailSchedule( std::vector<AccountData> & accounts );
        CGString EscapeString( const TCHAR * pszInput ) const;
        CGString FormatStringValue( const CGString & value ) const;
        CGString FormatOptionalStringValue( const CGString & value ) const;
        CGString FormatDateTimeValue( const CGString & value ) const;
        CGString FormatDateTimeValue( const CRealTime & value ) const;
        CGString FormatIPAddressValue( const CGString & value ) const;
        CGString FormatIPAddressValue( const struct in_addr & value ) const;
        unsigned int GetAccountId( const CGString & name );
        void UpdateAccountSyncTimestamp( const std::vector<AccountData> & accounts );
        CGString GetPrefixedTableName( const char * name ) const;

        void LogMySQLError( const char * context );

        MYSQL * m_pConnection;
        CGString m_sTablePrefix;
        CGString m_sDatabaseName;
        bool m_fAutoReconnect;
        int m_iReconnectTries;
        int m_iReconnectDelay;
        time_t m_tLastAccountSync;
};

#endif // _CWORLD_STORAGE_MYSQL_H_

