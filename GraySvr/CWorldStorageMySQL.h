#ifndef _CWORLD_STORAGE_MYSQL_H_
#define _CWORLD_STORAGE_MYSQL_H_

#include "../common/cstring.h"
#include <functional>
#include <vector>

class CAccount;
class CObjBase;
class CRealTime;
class CVarDefMap;
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

        class Transaction
        {
        public:
                explicit Transaction( CWorldStorageMySQL & storage, bool fAutoBegin = true );
                ~Transaction();

                bool Begin();
                bool Commit();
                void Rollback();
                bool IsActive() const
                {
                        return m_fActive;
                }

        private:
                CWorldStorageMySQL & m_Storage;
                bool m_fActive;
                bool m_fCommitted;
        };

        class UniversalRecord
        {
        public:
                explicit UniversalRecord( CWorldStorageMySQL & storage );
                UniversalRecord( CWorldStorageMySQL & storage, const CGString & table );

                void SetTable( const CGString & table );
                const CGString & GetTable() const
                {
                        return m_sTable;
                }

                void Clear();
                bool Empty() const;

                void SetRaw( const char * field, const CGString & expression );
                void SetNull( const char * field );
                void SetString( const char * field, const CGString & value );
                void SetOptionalString( const char * field, const CGString & value );
                void SetDateTime( const char * field, const CGString & value );
                void SetDateTime( const char * field, const CRealTime & value );
                void SetInt( const char * field, long long value );
                void SetUInt( const char * field, unsigned long long value );
                void SetBool( const char * field, bool value );

                CGString BuildInsert( bool fReplace, bool fUpdateOnDuplicate ) const;
                CGString BuildUpdate( const CGString & whereClause ) const;

        private:
                struct FieldEntry
                {
                        CGString m_sName;
                        CGString m_sValue;
                };

                FieldEntry * FindField( const char * field );
                const FieldEntry * FindField( const char * field ) const;
                void AddOrReplaceField( const char * field, const CGString & value );

                CWorldStorageMySQL & m_Storage;
                CGString m_sTable;
                std::vector<FieldEntry> m_Fields;
        };

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

        bool BeginTransaction();
        bool CommitTransaction();
        bool RollbackTransaction();
        bool WithTransaction( const std::function<bool()> & callback );

        bool LoadAllAccounts( std::vector<AccountData> & accounts );
        bool LoadChangedAccounts( std::vector<AccountData> & accounts );
        bool UpsertAccount( const CAccount & account );
        bool DeleteAccount( const TCHAR * pszAccountName );

        bool SaveWorldObject( CObjBase * pObject );
        bool SaveWorldObjects( const std::vector<CObjBase*> & objects );
        bool DeleteWorldObject( const CObjBase * pObject );
        bool ClearWorldData();

        const CGString & GetTablePrefix() const
        {
                return m_sTablePrefix;
        }

private:
        friend class Transaction;
        friend class UniversalRecord;

        bool Query( const CGString & query, MYSQL_RES ** ppResult = NULL );
        bool ExecuteQuery( const CGString & query );
        bool EnsureSchemaVersionTable();
        bool SetSchemaVersion( int version );
        bool ApplyMigration( int fromVersion );
        bool ApplyMigration_0_1();
        bool ApplyMigration_1_2();
        bool ApplyMigration_2_3();
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

        bool SaveWorldObjectInternal( CObjBase * pObject );
        bool SerializeWorldObject( CObjBase * pObject, CGString & outSerialized ) const;
        bool UpsertWorldObjectMeta( CObjBase * pObject, const CGString & serialized );
        bool UpsertWorldObjectData( const CObjBase * pObject, const CGString & serialized );
        bool RefreshWorldObjectComponents( const CObjBase * pObject );
        bool RefreshWorldObjectRelations( const CObjBase * pObject );
        void AppendVarDefComponents( const CGString & table, unsigned long long uid, const CVarDefMap * pMap, const TCHAR * pszComp, std::vector<UniversalRecord> & outRecords );
        CGString ComputeSerializedChecksum( const CGString & serialized ) const;
        bool ExecuteRecordsInsert( const std::vector<UniversalRecord> & records );
        bool ClearTable( const CGString & table );

        void LogMySQLError( const char * context );

        MYSQL * m_pConnection;
        CGString m_sTablePrefix;
        CGString m_sDatabaseName;
        bool m_fAutoReconnect;
        int m_iReconnectTries;
        int m_iReconnectDelay;
        time_t m_tLastAccountSync;
        int m_iTransactionDepth;
};

#endif // _CWORLD_STORAGE_MYSQL_H_

