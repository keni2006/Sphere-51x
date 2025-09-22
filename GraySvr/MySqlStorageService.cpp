#ifndef UNIT_TEST
#include "graysvr.h"
#else
#include "../tests/stubs/graysvr.h"
#endif
#include "MySqlStorageService.h"
#include "Storage/DirtyQueue.h"
#include "Storage/MySql/MySqlLogging.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <fstream>
#include <iomanip>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <functional>
#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <new>
#include <stdexcept>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Storage/MySql/MySqlConnection.h"

namespace
{
        static const int SCHEMA_VERSION_ROW = 1;      // Stores current schema version
        static const int SCHEMA_IMPORT_ROW = 2;       // Tracks legacy import state
        static const int SCHEMA_WORLD_SAVECOUNT_ROW = 3;
        static const int SCHEMA_WORLD_SAVEFLAG_ROW = 4;
        static const int CURRENT_SCHEMA_VERSION = 3;

        class TempFileGuard
        {
        public:
                TempFileGuard()
                {
                }

                explicit TempFileGuard( std::string path ) :
                        m_Path( std::move( path ))
                {
                }

                ~TempFileGuard()
                {
                        if ( ! m_Path.empty())
                        {
                                ::remove( m_Path.c_str());
                        }
                }

                TempFileGuard( const TempFileGuard & ) = delete;
                TempFileGuard & operator=( const TempFileGuard & ) = delete;

                TempFileGuard( TempFileGuard && other ) noexcept :
                        m_Path( std::move( other.m_Path ))
                {
                        other.m_Path.clear();
                }

                TempFileGuard & operator=( TempFileGuard && other ) noexcept
                {
                        if ( this != &other )
                        {
                                m_Path = std::move( other.m_Path );
                                other.m_Path.clear();
                        }
                        return *this;
                }

                const std::string & GetPath() const
                {
                        return m_Path;
                }

                bool IsValid() const
                {
                        return ! m_Path.empty();
                }

        private:
                std::string m_Path;
        };

bool GenerateTempScriptPath( std::string & outPath )
{
        char szBaseName[L_tmpnam];
        if ( tmpnam( szBaseName ) == NULL )
        {
                return false;
        }

        try
        {
                outPath.assign( szBaseName );
                outPath += ".scp";
        }
        catch ( const std::bad_alloc & )
        {
                return false;
        }

        return true;
}

bool IsSafeMariaDbIdentifierToken( const std::string & token )
{
        if ( token.empty())
        {
                return false;
        }

        for ( char ch : token )
        {
                unsigned char uch = static_cast<unsigned char>( ch );
                if (( uch != '_' ) && !std::isalnum( uch ))
                {
                        return false;
                }
        }

        return true;
}

std::string BuildSetNamesCommand( const std::string & charset, const std::string & collation )
{
        if ( !IsSafeMariaDbIdentifierToken( charset ))
        {
                return std::string();
        }

        std::string command = "SET NAMES '";
        command += charset;
        command += "'";

        if ( !collation.empty() && IsSafeMariaDbIdentifierToken( collation ))
        {
                command += " COLLATE '";
                command += collation;
                command += "'";
        }

        return command;
}

        std::string FormatByteString( const std::string & value )
        {
                if ( value.empty())
                {
                        return "(empty)";
                }

                std::ostringstream stream;
                stream << std::uppercase << std::hex << std::setfill( '0' );
                for ( size_t i = 0; i < value.size(); ++i )
                {
                        if ( i > 0 )
                        {
                                stream << ' ';
                        }
                        stream << "0x" << std::setw( 2 ) << static_cast<int>( static_cast<unsigned char>( value[i] ));
                }
                return stream.str();
        }

        void AppendUtf8( std::string & output, uint32_t codepoint )
        {
                if ( codepoint <= 0x7Fu )
                {
                        output.push_back( static_cast<char>( codepoint ));
                }
                else if ( codepoint <= 0x7FFu )
                {
                        output.push_back( static_cast<char>( 0xC0u | (( codepoint >> 6 ) & 0x1Fu )));
                        output.push_back( static_cast<char>( 0x80u | ( codepoint & 0x3Fu )));
                }
                else if ( codepoint <= 0xFFFFu )
                {
                        output.push_back( static_cast<char>( 0xE0u | (( codepoint >> 12 ) & 0x0Fu )));
                        output.push_back( static_cast<char>( 0x80u | (( codepoint >> 6 ) & 0x3Fu )));
                        output.push_back( static_cast<char>( 0x80u | ( codepoint & 0x3Fu )));
                }
                else if ( codepoint <= 0x10FFFFu )
                {
                        output.push_back( static_cast<char>( 0xF0u | (( codepoint >> 18 ) & 0x07u )));
                        output.push_back( static_cast<char>( 0x80u | (( codepoint >> 12 ) & 0x3Fu )));
                        output.push_back( static_cast<char>( 0x80u | (( codepoint >> 6 ) & 0x3Fu )));
                        output.push_back( static_cast<char>( 0x80u | ( codepoint & 0x3Fu )));
                }
        }

        bool IsValidUtf8( const std::string & value )
        {
                size_t i = 0;
                const size_t length = value.size();
                while ( i < length )
                {
                        unsigned char ch = static_cast<unsigned char>( value[i] );
                        if ( ch <= 0x7Fu )
                        {
                                ++i;
                                continue;
                        }

                        size_t remaining = 0;
                        uint32_t codepoint = 0;
                        if (( ch & 0xE0u ) == 0xC0u )
                        {
                                remaining = 1;
                                codepoint = ch & 0x1Fu;
                        }
                        else if (( ch & 0xF0u ) == 0xE0u )
                        {
                                remaining = 2;
                                codepoint = ch & 0x0Fu;
                        }
                        else if (( ch & 0xF8u ) == 0xF0u )
                        {
                                remaining = 3;
                                codepoint = ch & 0x07u;
                        }
                        else
                        {
                                return false;
                        }

                        if ( i + remaining >= length )
                        {
                                return false;
                        }

                        for ( size_t j = 0; j < remaining; ++j )
                        {
                                unsigned char continuation = static_cast<unsigned char>( value[i + j + 1] );
                                if (( continuation & 0xC0u ) != 0x80u )
                                {
                                        return false;
                                }
                                codepoint = static_cast<uint32_t>(( codepoint << 6 ) | ( continuation & 0x3Fu ));
                        }

                        switch ( remaining )
                        {
                        case 1:
                                if ( codepoint < 0x80u )
                                {
                                        return false;
                                }
                                break;
                        case 2:
                                if ( codepoint < 0x800u )
                                {
                                        return false;
                                }
                                break;
                        case 3:
                                if ( codepoint < 0x10000u )
                                {
                                        return false;
                                }
                                break;
                        default:
                                return false;
                        }

                        if ( codepoint > 0x10FFFFu )
                        {
                                return false;
                        }
                        if ( codepoint >= 0xD800u && codepoint <= 0xDFFFu )
                        {
                                return false;
                        }

                        i += remaining + 1;
                }

                return true;
        }

        std::string ConvertWindows1251ToUtf8( const std::string & value )
        {
                static const uint16_t WINDOWS1251_TABLE[128] =
                {
                        0x0402u, 0x0403u, 0x201Au, 0x0453u, 0x201Eu, 0x2026u, 0x2020u, 0x2021u,
                        0x20ACu, 0x2030u, 0x0409u, 0x2039u, 0x040Au, 0x040Cu, 0x040Bu, 0x040Fu,
                        0x0452u, 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x2022u, 0x2013u, 0x2014u,
                        0x0098u, 0x2122u, 0x0459u, 0x203Au, 0x045Au, 0x045Cu, 0x045Bu, 0x045Fu,
                        0x00A0u, 0x040Eu, 0x045Eu, 0x0408u, 0x00A4u, 0x0490u, 0x00A6u, 0x00A7u,
                        0x0401u, 0x00A9u, 0x0404u, 0x00ABu, 0x00ACu, 0x00ADu, 0x00AEu, 0x0407u,
                        0x00B0u, 0x00B1u, 0x0406u, 0x0456u, 0x0491u, 0x00B5u, 0x00B6u, 0x00B7u,
                        0x0451u, 0x2116u, 0x0454u, 0x00BBu, 0x0458u, 0x0405u, 0x0455u, 0x0457u,
                        0x0410u, 0x0411u, 0x0412u, 0x0413u, 0x0414u, 0x0415u, 0x0416u, 0x0417u,
                        0x0418u, 0x0419u, 0x041Au, 0x041Bu, 0x041Cu, 0x041Du, 0x041Eu, 0x041Fu,
                        0x0420u, 0x0421u, 0x0422u, 0x0423u, 0x0424u, 0x0425u, 0x0426u, 0x0427u,
                        0x0428u, 0x0429u, 0x042Au, 0x042Bu, 0x042Cu, 0x042Du, 0x042Eu, 0x042Fu,
                        0x0430u, 0x0431u, 0x0432u, 0x0433u, 0x0434u, 0x0435u, 0x0436u, 0x0437u,
                        0x0438u, 0x0439u, 0x043Au, 0x043Bu, 0x043Cu, 0x043Du, 0x043Eu, 0x043Fu,
                        0x0440u, 0x0441u, 0x0442u, 0x0443u, 0x0444u, 0x0445u, 0x0446u, 0x0447u,
                        0x0448u, 0x0449u, 0x044Au, 0x044Bu, 0x044Cu, 0x044Du, 0x044Eu, 0x044Fu
                };

                std::string output;
                output.reserve( value.size() * 2 );
                for ( unsigned char ch : value )
                {
                        if ( ch < 0x80u )
                        {
                                output.push_back( static_cast<char>( ch ));
                        }
                        else
                        {
                                uint32_t codepoint = WINDOWS1251_TABLE[ch - 0x80u];
                                AppendUtf8( output, codepoint );
                        }
                }
                return output;
        }

        std::string SanitizeIdentifierToken( const std::string & value )
        {
                std::string sanitized;
                sanitized.reserve( value.size());
                for ( unsigned char ch : value )
                {
                        if ( ch == '_' || std::isalnum( ch ))
                        {
                                sanitized.push_back( static_cast<char>( ch ));
                        }
                }
                return sanitized;
        }

        struct TablePrefixNormalizationResult
        {
                std::string m_sNormalized;
                bool m_fChanged;
                const char * m_pszReason;
        };

TablePrefixNormalizationResult NormalizeMySqlTablePrefix( const std::string & rawPrefix )
        {
                TablePrefixNormalizationResult result{ rawPrefix, false, NULL };
                if ( rawPrefix.empty())
                {
                        return result;
                }

                if ( IsValidUtf8( rawPrefix ))
                {
                        return result;
                }

                std::string converted = ConvertWindows1251ToUtf8( rawPrefix );
                if ( IsValidUtf8( converted ))
                {
                        result.m_sNormalized = std::move( converted );
                        result.m_fChanged = ( result.m_sNormalized != rawPrefix );
                        result.m_pszReason = "Converted Windows-1251 bytes to UTF-8";
                        return result;
                }

                std::string sanitized = SanitizeIdentifierToken( rawPrefix );
                result.m_fChanged = ( sanitized != rawPrefix );
                result.m_sNormalized = std::move( sanitized );
                result.m_pszReason = "Removed unsupported characters from table prefix";
                return result;
        }
}

namespace Storage
{
#if !defined(UNIT_TEST)
        class DirtyQueueProcessor
        {
        public:
                explicit DirtyQueueProcessor( MySqlStorageService & storage );
                ~DirtyQueueProcessor();

                DirtyQueueProcessor( const DirtyQueueProcessor & ) = delete;
                DirtyQueueProcessor & operator=( const DirtyQueueProcessor & ) = delete;

                void Schedule( MySqlStorageService::ObjectHandle handle, StorageDirtyType type );

        private:
                void Run( std::stop_token stopToken );
                void ProcessBatch( const DirtyQueue::Batch & batch );

                MySqlStorageService & m_Storage;
                DirtyQueue m_Queue;
                std::jthread m_Worker;
        };

        DirtyQueueProcessor::DirtyQueueProcessor( MySqlStorageService & storage ) :
                m_Storage( storage ),
                m_Worker( [this]( std::stop_token stopToken )
                {
                        Run( stopToken );
                })
        {
        }

        DirtyQueueProcessor::~DirtyQueueProcessor()
        {
        }

        void DirtyQueueProcessor::Schedule( MySqlStorageService::ObjectHandle handle, StorageDirtyType type )
        {
                m_Queue.Enqueue( handle, type );
        }

        void DirtyQueueProcessor::Run( std::stop_token stopToken )
        {
                DirtyQueue::Batch batch;
                while ( m_Queue.WaitForBatch( batch, stopToken ))
                {
                        ProcessBatch( batch );
                }
        }

        void DirtyQueueProcessor::ProcessBatch( const DirtyQueue::Batch & batch )
        {
                if ( batch.empty())
                {
                        return;
                }

                if ( !m_Storage.IsEnabled())
                {
                        return;
                }

                std::vector<CObjBase*> objects;
                objects.reserve( batch.size());

                for ( const auto & entry : batch )
                {
                        if ( entry.second != StorageDirtyType_Save )
                        {
                                continue;
                        }

                        CObjUID objUid( (UINT) entry.first );
                        CObjBase * pObject = objUid.ObjFind();
                        if ( pObject == NULL )
                        {
                                continue;
                        }

                        objects.push_back( pObject );
                }

                if ( objects.empty())
                {
                        return;
                }

                if ( m_Storage.SaveWorldObjects( objects ))
                {
                        return;
                }

                for ( const CObjBase * pObject : objects )
                {
                        if ( pObject == NULL )
                        {
                                continue;
                        }

                        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
                        g_Log.Event( LOGM_SAVE|LOGL_WARN, "Failed to persist object 0%llx to MySQL.", uid );
                }
        }
#endif // !UNIT_TEST

namespace Repository
{
        class PreparedStatementRepository
        {
        public:
                explicit PreparedStatementRepository( MySqlStorageService & storage ) :
                        m_Storage( storage )
                {
                }

                PreparedStatementRepository( const PreparedStatementRepository & ) = delete;
                PreparedStatementRepository & operator=( const PreparedStatementRepository & ) = delete;

        protected:
                bool ExecuteBatch( const std::string & query, size_t count,
                        const std::function<void(Storage::IDatabaseStatement &, size_t)> & binder ) const
                {
                        if ( count == 0 )
                        {
                                return true;
                        }

                        try
                        {
                                Storage::MySql::MySqlConnectionPool::ScopedConnection scoped;
                                Storage::MySql::MySqlConnection * connection = m_Storage.GetActiveConnection( scoped );
                                if ( connection == NULL )
                                {
                                        g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ),
                                                "MySQL prepared statement attempted without an active connection." );
                                        return false;
                                }

                                std::unique_ptr<Storage::IDatabaseStatement> statement = connection->Prepare( query );
                                for ( size_t i = 0; i < count; ++i )
                                {
                                        statement->Reset();
                                        binder( *statement, i );
                                        statement->Execute();
                                }

                                return true;
                        }
                        catch ( const Storage::DatabaseError & ex )
                        {
                                LogDatabaseError( ex, LOGL_ERROR );
                        }
                        catch ( const std::bad_alloc & )
                        {
                                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ),
                                        "Out of memory while preparing MySQL statement." );
                        }
                        return false;
                }

                MySqlStorageService & m_Storage;
        };

        struct WorldObjectMetaRecord
        {
                unsigned long long m_Uid = 0;
                std::string m_ObjectType;
                std::string m_ObjectSubtype;
                bool m_HasName = false;
                std::string m_Name;
                bool m_HasAccountId = false;
                unsigned int m_AccountId = 0;
                bool m_HasContainerUid = false;
                unsigned long long m_ContainerUid = 0;
                bool m_HasTopLevelUid = false;
                unsigned long long m_TopLevelUid = 0;
                bool m_HasPosX = false;
                bool m_HasPosY = false;
                bool m_HasPosZ = false;
                int m_PosX = 0;
                int m_PosY = 0;
                int m_PosZ = 0;
        };

        class WorldObjectMetaRepository : public PreparedStatementRepository
        {
        public:
                WorldObjectMetaRepository( MySqlStorageService & storage, const CGString & table ) :
                        PreparedStatementRepository( storage ),
                        m_Table( static_cast<const char *>( table ))
                {
                        const std::string quoted = "`" + m_Table + "`";
                        m_UpsertQuery =
                                "INSERT INTO " + quoted +
                                " (`uid`,`object_type`,`object_subtype`,`name`,`account_id`,`container_uid`,`top_level_uid`,`position_x`,`position_y`,`position_z`)";
                        m_UpsertQuery += " VALUES (?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE ";
                        m_UpsertQuery += "`object_type`=VALUES(`object_type`),";
                        m_UpsertQuery += "`object_subtype`=VALUES(`object_subtype`),";
                        m_UpsertQuery += "`name`=VALUES(`name`),";
                        m_UpsertQuery += "`account_id`=VALUES(`account_id`),";
                        m_UpsertQuery += "`container_uid`=VALUES(`container_uid`),";
                        m_UpsertQuery += "`top_level_uid`=VALUES(`top_level_uid`),";
                        m_UpsertQuery += "`position_x`=VALUES(`position_x`),";
                        m_UpsertQuery += "`position_y`=VALUES(`position_y`),";
                        m_UpsertQuery += "`position_z`=VALUES(`position_z`);";

                        m_DeleteQuery = "DELETE FROM " + quoted + " WHERE `uid` = ?;";
                }

                bool Upsert( const WorldObjectMetaRecord & record )
                {
                        return ExecuteBatch( m_UpsertQuery, 1, [&]( Storage::IDatabaseStatement & statement, size_t )
                        {
                                statement.BindUInt64( 0, record.m_Uid );
                                statement.BindString( 1, record.m_ObjectType );
                                statement.BindString( 2, record.m_ObjectSubtype );

                                if ( record.m_HasName )
                                {
                                        statement.BindString( 3, record.m_Name );
                                }
                                else
                                {
                                        statement.BindNull( 3 );
                                }

                                if ( record.m_HasAccountId )
                                {
                                        statement.BindUInt64( 4, static_cast<unsigned long long>( record.m_AccountId ));
                                }
                                else
                                {
                                        statement.BindNull( 4 );
                                }

                                if ( record.m_HasContainerUid )
                                {
                                        statement.BindUInt64( 5, record.m_ContainerUid );
                                }
                                else
                                {
                                        statement.BindNull( 5 );
                                }

                                if ( record.m_HasTopLevelUid )
                                {
                                        statement.BindUInt64( 6, record.m_TopLevelUid );
                                }
                                else
                                {
                                        statement.BindNull( 6 );
                                }

                                if ( record.m_HasPosX )
                                {
                                        statement.BindInt64( 7, record.m_PosX );
                                }
                                else
                                {
                                        statement.BindNull( 7 );
                                }

                                if ( record.m_HasPosY )
                                {
                                        statement.BindInt64( 8, record.m_PosY );
                                }
                                else
                                {
                                        statement.BindNull( 8 );
                                }

                                if ( record.m_HasPosZ )
                                {
                                        statement.BindInt64( 9, record.m_PosZ );
                                }
                                else
                                {
                                        statement.BindNull( 9 );
                                }
                        });
                }

                bool Delete( unsigned long long uid )
                {
                        return ExecuteBatch( m_DeleteQuery, 1, [&]( Storage::IDatabaseStatement & statement, size_t )
                        {
                                statement.BindUInt64( 0, uid );
                        });
                }

        private:
                std::string m_Table;
                std::string m_UpsertQuery;
                std::string m_DeleteQuery;
        };

        struct WorldObjectDataRecord
        {
                unsigned long long m_ObjectUid = 0;
                std::string m_Data;
                bool m_HasChecksum = false;
                std::string m_Checksum;
        };

        class WorldObjectDataRepository : public PreparedStatementRepository
        {
        public:
                WorldObjectDataRepository( MySqlStorageService & storage, const CGString & table ) :
                        PreparedStatementRepository( storage ),
                        m_Table( static_cast<const char *>( table ))
                {
                        const std::string quoted = "`" + m_Table + "`";
                        m_UpsertQuery = "REPLACE INTO " + quoted + " (`object_uid`,`data`,`checksum`) VALUES (?,?,?);";
                }

                bool Upsert( const WorldObjectDataRecord & record )
                {
                        return ExecuteBatch( m_UpsertQuery, 1, [&]( Storage::IDatabaseStatement & statement, size_t )
                        {
                                statement.BindUInt64( 0, record.m_ObjectUid );
                                statement.BindString( 1, record.m_Data );

                                if ( record.m_HasChecksum )
                                {
                                        statement.BindString( 2, record.m_Checksum );
                                }
                                else
                                {
                                        statement.BindNull( 2 );
                                }
                        });
                }

        private:
                std::string m_Table;
                std::string m_UpsertQuery;
        };

        struct WorldObjectComponentRecord
        {
                unsigned long long m_ObjectUid = 0;
                std::string m_Component;
                std::string m_Name;
                int m_Sequence = 0;
                bool m_HasValue = false;
                std::string m_Value;
        };

        class WorldObjectComponentRepository : public PreparedStatementRepository
        {
        public:
                WorldObjectComponentRepository( MySqlStorageService & storage, const CGString & table ) :
                        PreparedStatementRepository( storage ),
                        m_Table( static_cast<const char *>( table ))
                {
                        const std::string quoted = "`" + m_Table + "`";
                        m_DeleteQuery = "DELETE FROM " + quoted + " WHERE `object_uid` = ?;";
                        m_InsertQuery =
                                "INSERT INTO " + quoted + " (`object_uid`,`component`,`name`,`sequence`,`value`) VALUES (?,?,?,?,?);";
                }

                bool DeleteForObject( unsigned long long uid )
                {
                        return ExecuteBatch( m_DeleteQuery, 1, [&]( Storage::IDatabaseStatement & statement, size_t )
                        {
                                statement.BindUInt64( 0, uid );
                        });
                }

                bool InsertMany( const std::vector<WorldObjectComponentRecord> & records )
                {
                        return ExecuteBatch( m_InsertQuery, records.size(),
                                [&]( Storage::IDatabaseStatement & statement, size_t index )
                        {
                                const WorldObjectComponentRecord & record = records[index];
                                statement.BindUInt64( 0, record.m_ObjectUid );
                                statement.BindString( 1, record.m_Component );
                                statement.BindString( 2, record.m_Name );
                                statement.BindInt64( 3, record.m_Sequence );

                                if ( record.m_HasValue )
                                {
                                        statement.BindString( 4, record.m_Value );
                                }
                                else
                                {
                                        statement.BindNull( 4 );
                                }
                        });
                }

        private:
                std::string m_Table;
                std::string m_DeleteQuery;
                std::string m_InsertQuery;
        };

        struct WorldObjectRelationRecord
        {
                unsigned long long m_ParentUid = 0;
                unsigned long long m_ChildUid = 0;
                std::string m_Relation;
                int m_Sequence = 0;
        };

        class WorldObjectRelationRepository : public PreparedStatementRepository
        {
        public:
                WorldObjectRelationRepository( MySqlStorageService & storage, const CGString & table ) :
                        PreparedStatementRepository( storage ),
                        m_Table( static_cast<const char *>( table ))
                {
                        const std::string quoted = "`" + m_Table + "`";
                        m_DeleteQuery = "DELETE FROM " + quoted + " WHERE `parent_uid` = ? OR `child_uid` = ?;";
                        m_InsertQuery =
                                "INSERT INTO " + quoted + " (`parent_uid`,`child_uid`,`relation`,`sequence`) VALUES (?,?,?,?);";
                }

                bool DeleteForObject( unsigned long long uid )
                {
                        return ExecuteBatch( m_DeleteQuery, 1, [&]( Storage::IDatabaseStatement & statement, size_t )
                        {
                                statement.BindUInt64( 0, uid );
                                statement.BindUInt64( 1, uid );
                        });
                }

                bool InsertMany( const std::vector<WorldObjectRelationRecord> & records )
                {
                        return ExecuteBatch( m_InsertQuery, records.size(),
                                [&]( Storage::IDatabaseStatement & statement, size_t index )
                        {
                                const WorldObjectRelationRecord & record = records[index];
                                statement.BindUInt64( 0, record.m_ParentUid );
                                statement.BindUInt64( 1, record.m_ChildUid );
                                statement.BindString( 2, record.m_Relation );
                                statement.BindInt64( 3, record.m_Sequence );
                        });
                }

        private:
                std::string m_Table;
                std::string m_DeleteQuery;
                std::string m_InsertQuery;
        };
}
}

#if !defined(UNIT_TEST) || defined(UNIT_TEST_MYSQL_IMPLEMENTATION)

MySqlStorageService::Transaction::Transaction( MySqlStorageService & storage, bool fAutoBegin ) :
        m_Storage( storage ),
        m_fActive( false ),
        m_fCommitted( false )
{
        if ( fAutoBegin )
        {
                m_fActive = m_Storage.BeginTransaction();
        }
}

MySqlStorageService::Transaction::~Transaction()
{
        if ( m_fActive && ! m_fCommitted )
        {
                m_Storage.RollbackTransaction();
        }
}

bool MySqlStorageService::Transaction::Begin()
{
        if ( m_fActive )
        {
                return true;
        }
        m_fActive = m_Storage.BeginTransaction();
        return m_fActive;
}

bool MySqlStorageService::Transaction::Commit()
{
        if ( ! m_fActive )
        {
                return false;
        }
        if ( ! m_Storage.CommitTransaction())
        {
                return false;
        }
        m_fCommitted = true;
        m_fActive = false;
        return true;
}

void MySqlStorageService::Transaction::Rollback()
{
        if ( m_fActive )
        {
                m_Storage.RollbackTransaction();
                m_fActive = false;
        }
        m_fCommitted = false;
}

MySqlStorageService::UniversalRecord::UniversalRecord( MySqlStorageService & storage ) :
        m_Storage( storage )
{
}

MySqlStorageService::UniversalRecord::UniversalRecord( MySqlStorageService & storage, const CGString & table ) :
        m_Storage( storage ),
        m_sTable( table )
{
}

void MySqlStorageService::UniversalRecord::SetTable( const CGString & table )
{
        m_sTable = table;
}

void MySqlStorageService::UniversalRecord::Clear()
{
        m_Fields.clear();
}

bool MySqlStorageService::UniversalRecord::Empty() const
{
        return m_Fields.empty();
}

MySqlStorageService::UniversalRecord::FieldEntry * MySqlStorageService::UniversalRecord::FindField( const char * field )
{
        if ( field == NULL )
        {
                return NULL;
        }
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( m_Fields[i].m_sName.CompareNoCase( field ) == 0 )
                {
                        return &m_Fields[i];
                }
        }
        return NULL;
}

const MySqlStorageService::UniversalRecord::FieldEntry * MySqlStorageService::UniversalRecord::FindField( const char * field ) const
{
        if ( field == NULL )
        {
                return NULL;
        }
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( m_Fields[i].m_sName.CompareNoCase( field ) == 0 )
                {
                        return &m_Fields[i];
                }
        }
        return NULL;
}

void MySqlStorageService::UniversalRecord::AddOrReplaceField( const char * field, const CGString & value )
{
        if ( field == NULL )
        {
                return;
        }
        FieldEntry * pEntry = FindField( field );
        if ( pEntry != NULL )
        {
                pEntry->m_sValue = value;
                return;
        }

        FieldEntry entry;
        entry.m_sName = field;
        entry.m_sValue = value;
        m_Fields.push_back( entry );
}

void MySqlStorageService::UniversalRecord::SetRaw( const char * field, const CGString & expression )
{
        AddOrReplaceField( field, expression );
}

void MySqlStorageService::UniversalRecord::SetNull( const char * field )
{
        AddOrReplaceField( field, CGString( "NULL" ));
}

void MySqlStorageService::UniversalRecord::SetString( const char * field, const CGString & value )
{
        AddOrReplaceField( field, m_Storage.FormatStringValue( value ));
}

void MySqlStorageService::UniversalRecord::SetOptionalString( const char * field, const CGString & value )
{
        AddOrReplaceField( field, m_Storage.FormatOptionalStringValue( value ));
}

void MySqlStorageService::UniversalRecord::SetDateTime( const char * field, const CGString & value )
{
        AddOrReplaceField( field, m_Storage.FormatDateTimeValue( value ));
}

void MySqlStorageService::UniversalRecord::SetDateTime( const char * field, const CRealTime & value )
{
        AddOrReplaceField( field, m_Storage.FormatDateTimeValue( value ));
}

void MySqlStorageService::UniversalRecord::SetInt( const char * field, long long value )
{
        CGString sValue;
#ifdef _WIN32
        sValue.Format( "%I64d", value );
#else
        sValue.Format( "%lld", value );
#endif
        AddOrReplaceField( field, sValue );
}

void MySqlStorageService::UniversalRecord::SetUInt( const char * field, unsigned long long value )
{
        CGString sValue;
#ifdef _WIN32
        sValue.Format( "%I64u", value );
#else
        sValue.Format( "%llu", value );
#endif
        AddOrReplaceField( field, sValue );
}

void MySqlStorageService::UniversalRecord::SetBool( const char * field, bool value )
{
        AddOrReplaceField( field, CGString( value ? "1" : "0" ));
}

CGString MySqlStorageService::UniversalRecord::BuildInsert( bool fReplace, bool fUpdateOnDuplicate ) const
{
        CGString sQuery;
        if ( m_sTable.IsEmpty() || m_Fields.empty())
        {
                return sQuery;
        }

        CGString sColumns;
        CGString sValues;
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( ! sColumns.IsEmpty())
                {
                        sColumns += ",";
                        sValues += ",";
                }
                CGString sColumn;
                sColumn.Format( "`%s`", (const char *) m_Fields[i].m_sName );
                sColumns += sColumn;
                sValues += m_Fields[i].m_sValue;
        }

        const char * pszVerb = fReplace ? "REPLACE INTO" : "INSERT INTO";
        sQuery.Format( "%s `%s` (%s) VALUES (%s)", pszVerb, (const char *) m_sTable, (const char *) sColumns, (const char *) sValues );

        if ( ! fReplace && fUpdateOnDuplicate && ! m_Fields.empty())
        {
                sQuery += " ON DUPLICATE KEY UPDATE ";
                for ( size_t i = 0; i < m_Fields.size(); ++i )
                {
                        if ( i > 0 )
                        {
                                sQuery += ",";
                        }
                        CGString sUpdate;
                        sUpdate.Format( "`%s`=VALUES(`%s`)", (const char *) m_Fields[i].m_sName, (const char *) m_Fields[i].m_sName );
                        sQuery += sUpdate;
                }
        }

        sQuery += ";";
        return sQuery;
}

CGString MySqlStorageService::UniversalRecord::BuildUpdate( const CGString & whereClause ) const
{
        CGString sQuery;
        if ( m_sTable.IsEmpty() || m_Fields.empty())
        {
                return sQuery;
        }

        CGString sAssignments;
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( ! sAssignments.IsEmpty())
                {
                        sAssignments += ",";
                }
                CGString sAssign;
                sAssign.Format( "`%s`=%s", (const char *) m_Fields[i].m_sName, (const char *) m_Fields[i].m_sValue );
                sAssignments += sAssign;
        }

        sQuery.Format( "UPDATE `%s` SET %s", (const char *) m_sTable, (const char *) sAssignments );
        if ( ! whereClause.IsEmpty())
        {
                sQuery += " ";
                sQuery += whereClause;
        }
        sQuery += ";";
        return sQuery;
}

#endif // !UNIT_TEST || UNIT_TEST_MYSQL_IMPLEMENTATION

MySqlStorageService::MySqlStorageService() :
        m_tLastAccountSync( 0 )
{
        m_sTablePrefix.Empty();
        m_sDatabaseName.Empty();
        m_sTableCharset.Empty();
        m_sTableCollation.Empty();
}

MySqlStorageService::~MySqlStorageService()
{
        Stop();
}

bool MySqlStorageService::Start( const CServerMySQLConfig & config )
{
        Stop();

        if ( !config.m_fEnable )
        {
                return false;
        }

        std::string rawTablePrefix;
        if ( !config.m_sTablePrefix.IsEmpty())
        {
                rawTablePrefix.assign( static_cast<const char *>( config.m_sTablePrefix ));
        }

        TablePrefixNormalizationResult prefixNormalization = NormalizeMySqlTablePrefix( rawTablePrefix );
        if ( prefixNormalization.m_fChanged )
        {
                const char * reason = ( prefixNormalization.m_pszReason != NULL ) ? prefixNormalization.m_pszReason : "Normalized MySQL table prefix";
                std::string normalizedLogValue = prefixNormalization.m_sNormalized.empty() ? std::string( "(empty)" ) : prefixNormalization.m_sNormalized;
                g_Log.Event( LOGM_INIT|LOGL_WARN,
                        "Normalized MySQL table prefix bytes (%s) to '%s' (%s).",
                        FormatByteString( rawTablePrefix ).c_str(),
                        normalizedLogValue.c_str(),
                        reason );
        }

        m_sTablePrefix = prefixNormalization.m_sNormalized.c_str();
        m_sDatabaseName = config.m_sDatabase;
        m_sTableCharset.Empty();
        m_sTableCollation.Empty();
        m_tLastAccountSync = 0;

        Storage::MySql::ConnectionManager::ConnectionDetails connectionDetails;
        if ( !m_ConnectionManager.Connect( config, connectionDetails ))
        {
                return false;
        }

        if ( !connectionDetails.m_TableCharset.empty())
        {
                m_sTableCharset = connectionDetails.m_TableCharset.c_str();
        }
        else
        {
                m_sTableCharset.Empty();
        }

        if ( !connectionDetails.m_TableCollation.empty())
        {
                m_sTableCollation = connectionDetails.m_TableCollation.c_str();
        }
        else
        {
                m_sTableCollation.Empty();
        }

#if defined(UNIT_TEST)
        if ( !EnsureSchemaVersionTable())
#else
        if ( !EnsureSchema())
#endif
        {
                Stop();
                return false;
        }

#ifndef UNIT_TEST
        m_DirtyProcessor = std::make_unique<Storage::DirtyQueueProcessor>( *this );
#endif

        return true;
}

void MySqlStorageService::Stop()
{
#ifndef UNIT_TEST
        m_DirtyProcessor.reset();
#endif
        m_ConnectionManager.Disconnect();
        m_sTablePrefix.Empty();
        m_sDatabaseName.Empty();
        m_sTableCharset.Empty();
        m_sTableCollation.Empty();
        m_tLastAccountSync = 0;
}

CGString MySqlStorageService::BuildSchemaVersionCreateQuery() const
{
        CGString sTableName;
        sTableName.Format( "%s%s", (const char *) m_sTablePrefix, "schema_version" );

        const char * pszCharset = m_sTableCharset.IsEmpty() ? "utf8mb4" : (const char *) m_sTableCharset;
        CGString sCollationSuffix;
        if ( !m_sTableCollation.IsEmpty())
        {
                sCollationSuffix.Format( " COLLATE=%s", (const char *) m_sTableCollation );
        }

        CGString sQuery;
        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT NOT NULL,"
                "`version` INT NOT NULL,"
                "PRIMARY KEY (`id`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sTableName,
                pszCharset,
                (const char *) sCollationSuffix );
        return sQuery;
}

#ifdef UNIT_TEST
bool MySqlStorageService::DebugExecuteQuery( const CGString & query )
{
        try
        {
                Storage::MySql::MySqlConnectionPool::ScopedConnection scoped;
                Storage::MySql::MySqlConnection * connection = GetActiveConnection( scoped );
                if ( connection == NULL )
                {
                        return false;
                }
                std::string sql( (const char *) query );
                connection->Execute( sql );
                return true;
        }
        catch ( const Storage::DatabaseError & ex )
        {
                LogDatabaseError( ex, LOGL_ERROR );
        }
        return false;
}
#endif

CGString MySqlStorageService::EscapeString( const TCHAR * pszInput ) const
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

        if ( ! IsConnected())
        {
                sResult = pszInput;
                return sResult;
        }

        try
        {
                Storage::MySql::MySqlConnectionPool::ScopedConnection scopedConnection;
                Storage::MySql::MySqlConnection * connection = GetActiveConnection( scopedConnection );
                if ( connection == NULL )
                {
                        return sResult;
                }
                std::string escaped = connection->EscapeString( pszInput, uiLength );
                sResult = escaped.c_str();
                return sResult;
        }
        catch ( const Storage::DatabaseError & ex )
        {
                LogDatabaseError( ex, LOGL_ERROR );
        }

        return sResult;
}

CGString MySqlStorageService::FormatStringValue( const CGString & value ) const
{
        CGString sEscaped = EscapeString( (const TCHAR *) value );
        CGString sResult;
        sResult.Format( "'%s'", (const char *) sEscaped );
        return sResult;
}

CGString MySqlStorageService::FormatOptionalStringValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString MySqlStorageService::FormatDateTimeValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString MySqlStorageService::FormatDateTimeValue( const CRealTime & value ) const
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

CGString MySqlStorageService::FormatIPAddressValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString MySqlStorageService::FormatIPAddressValue( const struct in_addr & value ) const
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

#if !defined(UNIT_TEST) || defined(UNIT_TEST_MYSQL_IMPLEMENTATION)

bool MySqlStorageService::IsConnected() const
{
        return m_ConnectionManager.IsConnected();
}

bool MySqlStorageService::IsEnabled() const
{
        return m_ConnectionManager.IsConnected();
}

CGString MySqlStorageService::GetPrefixedTableName( const char * name ) const
{
        CGString sName;
        sName.Format( "%s%s", (const char *) m_sTablePrefix, name );
        return sName;
}

const char * MySqlStorageService::GetDefaultTableCharset() const
{
        if ( ! m_sTableCharset.IsEmpty())
        {
                return m_sTableCharset;
        }
        return "utf8mb4";
}

const char * MySqlStorageService::GetDefaultTableCollation() const
{
        if ( ! m_sTableCollation.IsEmpty())
        {
                return m_sTableCollation;
        }
        return NULL;
}

CGString MySqlStorageService::GetDefaultTableCollationSuffix() const
{
        CGString sSuffix;
        const char * pszCollation = GetDefaultTableCollation();
        if ( pszCollation != NULL && pszCollation[0] != '\0' )
        {
                sSuffix.Format( " COLLATE=%s", pszCollation );
        }
        return sSuffix;
}

Storage::MySql::MySqlConnection * MySqlStorageService::GetActiveConnection( Storage::MySql::MySqlConnectionPool::ScopedConnection & scoped ) const
{
        return m_ConnectionManager.GetActiveConnection( scoped );
}

bool MySqlStorageService::Query( const CGString & query, std::unique_ptr<Storage::IDatabaseResult> * pResult )
{
        if ( ! IsConnected())
        {
                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "MySQL query attempted without an active connection." );
                return false;
        }

        try
        {
                Storage::MySql::MySqlConnectionPool::ScopedConnection scopedConnection;
                Storage::MySql::MySqlConnection * connection = GetActiveConnection( scopedConnection );
                if ( connection == NULL )
                {
                        return false;
                }

                const char * pszQuery = (const char *) query;
                if ( pResult != NULL )
                {
                        std::unique_ptr<Storage::IDatabaseResult> result = connection->Query( pszQuery );
                        if ( pResult != NULL )
                        {
                                *pResult = std::move( result );
                        }
                }
                else
                {
                        connection->Execute( pszQuery );
                }
                return true;
        }
        catch ( const Storage::DatabaseError & ex )
        {
                LogDatabaseError( ex, LOGL_ERROR );
                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "Failed query: %s", (const char *) query );
        }
        return false;
}

bool MySqlStorageService::ExecuteQuery( const CGString & query )
{
        return Query( query, NULL );
}

bool MySqlStorageService::ExecuteRecordsInsert( const std::vector<UniversalRecord> & records )
{
        if ( records.empty())
        {
                return true;
        }

        for ( size_t i = 0; i < records.size(); ++i )
        {
                CGString sQuery = records[i].BuildInsert( false, false );
                if ( sQuery.IsEmpty())
                {
                        return false;
                }
                if ( ! ExecuteQuery( sQuery ))
                {
                        return false;
                }
        }

        return true;
}

bool MySqlStorageService::ClearTable( const CGString & table )
{
        if ( table.IsEmpty())
        {
                return false;
        }

        CGString sQuery;
        sQuery.Format( "DELETE FROM `%s`;", (const char *) table );
        return ExecuteQuery( sQuery );
}

bool MySqlStorageService::BeginTransaction()
{
        return m_ConnectionManager.BeginTransaction();
}

bool MySqlStorageService::CommitTransaction()
{
        return m_ConnectionManager.CommitTransaction();
}

bool MySqlStorageService::RollbackTransaction()
{
        return m_ConnectionManager.RollbackTransaction();
}

bool MySqlStorageService::WithTransaction( const std::function<bool()> & callback )
{
        if ( ! callback )
        {
                return false;
        }

        Transaction transaction( *this );
        if ( ! transaction.IsActive())
        {
                if ( ! transaction.Begin())
                {
                        return false;
                }
        }

        bool fResult = false;
        try
        {
                fResult = callback();
        }
        catch (...)
        {
                transaction.Rollback();
                throw;
        }

        if ( fResult )
        {
                if ( transaction.Commit())
                {
                        return true;
                }
                transaction.Rollback();
                return false;
        }

        transaction.Rollback();
        return false;
}

#endif // !UNIT_TEST || UNIT_TEST_MYSQL_IMPLEMENTATION

#if !defined(UNIT_TEST) || defined(UNIT_TEST_MYSQL_IMPLEMENTATION)

CGString MySqlStorageService::ComputeSerializedChecksum( const CGString & serialized ) const
{
        const char * pData = (const char *) serialized;
        size_t len = (size_t) serialized.GetLength();

        unsigned long long hash = 1469598103934665603ULL;
        for ( size_t i = 0; i < len; ++i )
        {
                hash ^= (unsigned char) pData[i];
                hash *= 1099511628211ULL;
        }

        CGString sHash;
#ifdef _WIN32
        sHash.Format( "%016I64x", hash );
#else
        sHash.Format( "%016llx", hash );
#endif
        return sHash;
}

unsigned int MySqlStorageService::GetAccountId( const CGString & name )
{
        if ( ! IsConnected())
        {
                return 0;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sEscName = EscapeString( (const TCHAR *) name );

        CGString sQuery;
        sQuery.Format( "SELECT `id` FROM `%s` WHERE `name` = '%s' LIMIT 1;", (const char *) sAccounts, (const char *) sEscName );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return 0;
        }

        unsigned int uiId = 0;
        if ( result && result->IsValid())
        {
                Storage::IDatabaseResult::Row pRow = result->FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        uiId = (unsigned int) strtoul( pRow[0], NULL, 10 );
                }
        }
        return uiId;
}

bool MySqlStorageService::FetchAccounts( std::vector<AccountData> & accounts, const CGString & whereClause )
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

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result || !result->IsValid())
        {
            return true;
        }

        Storage::IDatabaseResult::Row pRow;
        while (( pRow = result->FetchRow()) != NULL )
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

        return true;
}

void MySqlStorageService::LoadAccountEmailSchedule( std::vector<AccountData> & accounts )
{
        if ( accounts.empty())
        {
                return;
        }

        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sQuery;
        sQuery.Format( "SELECT `account_id`,`sequence`,`message_id` FROM `%s` ORDER BY `account_id`,`sequence`;", (const char *) sEmails );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return;
        }

        if ( !result || !result->IsValid())
        {
                return;
        }

        std::unordered_map<unsigned int, AccountData*> mapAccounts;
        for ( size_t i = 0; i < accounts.size(); ++i )
        {
                accounts[i].m_EmailSchedule.clear();
                mapAccounts[ accounts[i].m_id ] = &accounts[i];
        }

        Storage::IDatabaseResult::Row pRow;
        while (( pRow = result->FetchRow()) != NULL )
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

}

void MySqlStorageService::UpdateAccountSyncTimestamp( const std::vector<AccountData> & accounts )
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

bool MySqlStorageService::LoadAllAccounts( std::vector<AccountData> & accounts )
{
        if ( ! FetchAccounts( accounts, CGString()))
        {
                return false;
        }
        LoadAccountEmailSchedule( accounts );
        UpdateAccountSyncTimestamp( accounts );
        return true;
}

bool MySqlStorageService::LoadChangedAccounts( std::vector<AccountData> & accounts )
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

bool MySqlStorageService::UpsertAccount( const CAccount & account )
{
        if ( ! IsConnected())
        {
                return false;
        }

        Transaction transaction( *this );
        if ( ! transaction.Begin())
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        UniversalRecord accountRecord( *this, sAccounts );

        CGString sName = CGString( account.GetName());
        accountRecord.SetString( "name", sName );
        accountRecord.SetString( "password", CGString( account.GetPassword()));
        accountRecord.SetInt( "plevel", account.GetPrivLevel());
        accountRecord.SetUInt( "priv_flags", (unsigned int) account.m_PrivFlags );

        int statusValue = 0;
        if ( account.IsPriv( PRIV_BLOCKED ))
        {
                statusValue |= 0x1;
        }
        if ( account.IsPriv( PRIV_JAILED ))
        {
                statusValue |= 0x2;
        }
        accountRecord.SetInt( "status", statusValue );

        accountRecord.SetOptionalString( "comment", account.m_sComment );
        accountRecord.SetOptionalString( "email", account.m_sEMail );
        accountRecord.SetOptionalString( "chat_name", account.m_sChatName );

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
        accountRecord.SetOptionalString( "language", sLanguageRaw );
        accountRecord.SetInt( "total_connect_time", account.m_Total_Connect_Time );
        accountRecord.SetInt( "last_connect_time", account.m_Last_Connect_Time );
        accountRecord.SetRaw( "last_ip", FormatIPAddressValue( account.m_Last_IP ));
        accountRecord.SetRaw( "first_ip", FormatIPAddressValue( account.m_First_IP ));
        accountRecord.SetDateTime( "last_login", account.m_Last_Connect_Date );
        accountRecord.SetDateTime( "first_login", account.m_First_Connect_Date );

        if ( account.m_uidLastChar.IsValidUID())
        {
                accountRecord.SetUInt( "last_char_uid", (unsigned int) account.m_uidLastChar );
        }
        else
        {
                accountRecord.SetNull( "last_char_uid" );
        }

        accountRecord.SetUInt( "email_failures", (unsigned int) account.m_iEmailFailures );

        if ( ! ExecuteQuery( accountRecord.BuildInsert( false, true )))
        {
                transaction.Rollback();
                return false;
        }

        unsigned int accountId = GetAccountId( sName );
        if ( accountId == 0 )
        {
                transaction.Rollback();
                return false;
        }

        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sDelete;
        sDelete.Format( "DELETE FROM `%s` WHERE `account_id` = %u;", (const char *) sEmails, accountId );
        if ( ! ExecuteQuery( sDelete ))
        {
                transaction.Rollback();
                return false;
        }

        std::vector<UniversalRecord> emailRecords;
        for ( int i = 0; i < account.m_EMailSchedule.GetCount(); ++i )
        {
                UniversalRecord emailRecord( *this, sEmails );
                emailRecord.SetUInt( "account_id", accountId );
                emailRecord.SetInt( "sequence", i );
                emailRecord.SetUInt( "message_id", (unsigned int) account.m_EMailSchedule[i] );
                emailRecords.push_back( emailRecord );
        }

        if ( ! emailRecords.empty())
        {
                if ( ! ExecuteRecordsInsert( emailRecords ))
                {
                        transaction.Rollback();
                        return false;
                }
        }

        if ( ! transaction.Commit())
        {
                transaction.Rollback();
                return false;
        }

        return true;
}

bool MySqlStorageService::DeleteAccount( const TCHAR * pszAccountName )
{
        if ( ! IsConnected() || pszAccountName == NULL || pszAccountName[0] == '\0' )
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sEscName = EscapeString( pszAccountName );

        CGString sQuery;
        sQuery.Format( "DELETE FROM `%s` WHERE `account_id` IN (SELECT `id` FROM `%s` WHERE `name` = '%s');",
                (const char *) sEmails, (const char *) sAccounts, (const char *) sEscName );
        if ( !ExecuteQuery( sQuery ))
        {
                return false;
        }

        CGString sDeleteAccount;
        sDeleteAccount.Format( "DELETE FROM `%s` WHERE `name` = '%s';", (const char *) sAccounts, (const char *) sEscName );
        return ExecuteQuery( sDeleteAccount );
}

bool MySqlStorageService::SaveWorldObject( CObjBase * pObject )
{
        if ( ! IsConnected() || pObject == NULL )
        {
                return false;
        }

        return WithTransaction( [this, pObject]() -> bool
        {
                return SaveWorldObjectInternal( pObject );
        });
}

bool MySqlStorageService::SaveWorldObjects( const std::vector<CObjBase*> & objects )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( objects.empty())
        {
                        return true;
        }

        return WithTransaction( [this, &objects]() -> bool
        {
                for ( size_t i = 0; i < objects.size(); ++i )
                {
                        CObjBase * pObject = objects[i];
                        if ( pObject == NULL )
                        {
                                continue;
                        }
                        if ( ! SaveWorldObjectInternal( pObject ))
                        {
                                return false;
                        }
                }
                return true;
        });
}

bool MySqlStorageService::DeleteWorldObject( const CObjBase * pObject )
{
        if ( ! IsConnected() || pObject == NULL )
        {
                return false;
        }

        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
        const CGString sWorldObjects = GetPrefixedTableName( "world_objects" );

        return WithTransaction( [this, uid, sWorldObjects]() -> bool
        {
                Storage::Repository::WorldObjectMetaRepository repository( *this, sWorldObjects );
                return repository.Delete( uid );
        });
}

bool MySqlStorageService::DeleteObject( const CObjBase * pObject )
{
        return DeleteWorldObject( pObject );
}

#ifndef UNIT_TEST
void MySqlStorageService::ScheduleSave( ObjectHandle handle, StorageDirtyType type )
{
        if ( type == StorageDirtyType_None )
        {
                return;
        }

        if ( ! IsEnabled())
        {
                return;
        }

        if ( m_DirtyProcessor )
        {
                m_DirtyProcessor->Schedule( handle, type );
        }
}
#endif

#ifndef UNIT_TEST
bool MySqlStorageService::SaveSector( const CSector & sector )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureSectorColumns())
        {
                return false;
        }

        const CGString sSectors = GetPrefixedTableName( "sectors" );
        UniversalRecord record( *this, sSectors );

        const CPointMap base = sector.GetBase();
        record.SetInt( "map_plane", 0 );
        record.SetInt( "x1", base.m_x );
        record.SetInt( "y1", base.m_y );
        record.SetInt( "x2", base.m_x + SECTOR_SIZE_X );
        record.SetInt( "y2", base.m_y + SECTOR_SIZE_Y );
        record.SetRaw( "last_update", "CURRENT_TIMESTAMP" );

        if ( sector.IsLightOverriden())
        {
                record.SetBool( "has_light_override", true );
                record.SetInt( "local_light", sector.GetLight());
        }
        else
        {
                record.SetBool( "has_light_override", false );
                record.SetNull( "local_light" );
        }

        if ( sector.IsRainOverriden())
        {
                record.SetBool( "has_rain_override", true );
                record.SetInt( "rain_chance", sector.GetRainChance());
        }
        else
        {
                record.SetBool( "has_rain_override", false );
                record.SetNull( "rain_chance" );
        }

        if ( sector.IsColdOverriden())
        {
                record.SetBool( "has_cold_override", true );
                record.SetInt( "cold_chance", sector.GetColdChance());
        }
        else
        {
                record.SetBool( "has_cold_override", false );
                record.SetNull( "cold_chance" );
        }

        return ExecuteQuery( record.BuildInsert( false, true ));
}
#endif

bool MySqlStorageService::SaveChar( CChar & character )
{
        return SaveWorldObject( &character );
}

bool MySqlStorageService::SaveItem( CItem & item )
{
        return SaveWorldObject( &item );
}

#ifndef UNIT_TEST
bool MySqlStorageService::SaveGMPage( const CGMPage & page )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureGMPageColumns())
        {
                return false;
        }

        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        UniversalRecord record( *this, sGMPages );

        record.SetNull( "account_id" );
        record.SetNull( "account_name" );

        CAccount * pAccount = page.FindAccount();
        if ( pAccount != NULL )
        {
                CGString sName = pAccount->GetName();
                if ( ! sName.IsEmpty())
                {
                        unsigned int accountId = GetAccountId( sName );
                        if ( accountId > 0 )
                        {
                                record.SetUInt( "account_id", accountId );
                        }
                        record.SetOptionalString( "account_name", sName );
                }
        }

        record.SetNull( "character_uid" );
        record.SetOptionalString( "reason", CGString( page.GetReason()));
        record.SetUInt( "status", 0 );
        record.SetInt( "page_time", (long long) page.m_lTime );
        record.SetInt( "pos_x", page.m_p.m_x );
        record.SetInt( "pos_y", page.m_p.m_y );
        record.SetInt( "pos_z", page.m_p.m_z );
        record.SetInt( "map_plane", 0 );

        return ExecuteQuery( record.BuildInsert( false, true ));
}

bool MySqlStorageService::SaveServer( const CServRef & server )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureServerColumns())
        {
                return false;
        }

        const TCHAR * pszName = server.GetName();
        if ( pszName == NULL || pszName[0] == '\0' )
        {
                return false;
        }

        const CGString sServers = GetPrefixedTableName( "servers" );
        UniversalRecord record( *this, sServers );

        record.SetString( "name", CGString( pszName ));
        record.SetOptionalString( "address", CGString( server.m_ip.GetAddrStr()));
        record.SetInt( "port", server.m_ip.GetPort());
        record.SetInt( "status", server.IsValidStatus() ? 1 : 0 );
        record.SetInt( "time_zone", server.m_TimeZone );
        record.SetInt( "clients_avg", server.GetClientsAvg());
        record.SetOptionalString( "url", server.m_sURL );
        record.SetOptionalString( "email", server.m_sEMail );
        record.SetOptionalString( "register_password", server.m_sRegisterPassword );
        record.SetOptionalString( "notes", server.m_sNotes );
        record.SetOptionalString( "language", server.m_sLang );
        record.SetOptionalString( "version", server.m_sVersion );
        record.SetInt( "acc_app", server.m_eAccApp );
        record.SetInt( "last_valid_seconds", server.GetTimeSinceLastValid());
        record.SetInt( "age_hours", server.GetAgeHours());
        record.SetRaw( "last_seen", "CURRENT_TIMESTAMP" );

        return ExecuteQuery( record.BuildInsert( false, true ));
}
#endif

bool MySqlStorageService::ClearGMPages()
{
        if ( ! IsConnected())
        {
                return false;
        }
        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        return ClearTable( sGMPages );
}

bool MySqlStorageService::ClearServers()
{
        if ( ! IsConnected())
        {
                return false;
        }
        const CGString sServers = GetPrefixedTableName( "servers" );
        return ClearTable( sServers );
}

bool MySqlStorageService::ClearWorldData()
{
        if ( ! IsConnected())
        {
                return false;
        }

        return WithTransaction( [this]() -> bool
        {
                const CGString sAudit = GetPrefixedTableName( "world_object_audit" );
                const CGString sRelations = GetPrefixedTableName( "world_object_relations" );
                const CGString sComponents = GetPrefixedTableName( "world_object_components" );
                const CGString sData = GetPrefixedTableName( "world_object_data" );
                const CGString sObjects = GetPrefixedTableName( "world_objects" );
                const CGString sSavepoints = GetPrefixedTableName( "world_savepoints" );

                if ( ! ClearTable( sAudit ))
                {
                        return false;
                }
                if ( ! ClearTable( sRelations ))
                {
                        return false;
                }
                if ( ! ClearTable( sComponents ))
                {
                        return false;
                }
                if ( ! ClearTable( sData ))
                {
                        return false;
                }
                if ( ! ClearTable( sObjects ))
                {
                        return false;
                }
                if ( ! ClearTable( sSavepoints ))
                {
                        return false;
                }
                return true;
        });
}

bool MySqlStorageService::LoadSectors( std::vector<SectorData> & sectors )
{
        sectors.clear();
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureSectorColumns())
        {
                return false;
        }

        const CGString sSectors = GetPrefixedTableName( "sectors" );
        CGString sQuery;
        sQuery.Format( "SELECT `map_plane`,`x1`,`y1`,`x2`,`y2`,`has_light_override`,`local_light`,`has_rain_override`,`rain_chance`,`has_cold_override`,`cold_chance` FROM `%s`;",
                (const char *) sSectors );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result || !result->IsValid())
        {
                return true;
        }

        Storage::IDatabaseResult::Row pRow;
        while (( pRow = result->FetchRow()) != NULL )
        {
                SectorData data;
                data.m_iMapPlane = pRow[0] ? atoi( pRow[0] ) : 0;
                data.m_iX1 = pRow[1] ? atoi( pRow[1] ) : 0;
                data.m_iY1 = pRow[2] ? atoi( pRow[2] ) : 0;
                data.m_iX2 = pRow[3] ? atoi( pRow[3] ) : 0;
                data.m_iY2 = pRow[4] ? atoi( pRow[4] ) : 0;
                data.m_fHasLightOverride = pRow[5] ? ( atoi( pRow[5] ) != 0 ) : false;
                data.m_iLocalLight = pRow[6] ? atoi( pRow[6] ) : 0;
                data.m_fHasRainOverride = pRow[7] ? ( atoi( pRow[7] ) != 0 ) : false;
                data.m_iRainChance = pRow[8] ? atoi( pRow[8] ) : 0;
                data.m_fHasColdOverride = pRow[9] ? ( atoi( pRow[9] ) != 0 ) : false;
                data.m_iColdChance = pRow[10] ? atoi( pRow[10] ) : 0;
                sectors.push_back( data );
        }

        return true;
}

bool MySqlStorageService::LoadWorldObjects( std::vector<WorldObjectRecord> & objects )
{
        objects.clear();
        if ( ! IsConnected())
        {
                return false;
        }

        const CGString sObjects = GetPrefixedTableName( "world_objects" );
        const CGString sData = GetPrefixedTableName( "world_object_data" );

        CGString sQuery;
        sQuery.Format(
                "SELECT o.`uid`,o.`object_type`,o.`object_subtype`,d.`data` FROM `%s` o INNER JOIN `%s` d ON o.`uid` = d.`object_uid` ORDER BY o.`uid`;",
                (const char *) sObjects, (const char *) sData );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result || !result->IsValid())
        {
                return true;
        }

        Storage::IDatabaseResult::Row pRow;
        while (( pRow = result->FetchRow()) != NULL )
        {
                WorldObjectRecord record;
#ifdef _WIN32
                record.m_uid = pRow[0] ? (unsigned long long) _strtoui64( pRow[0], NULL, 10 ) : 0;
#else
                record.m_uid = pRow[0] ? (unsigned long long) strtoull( pRow[0], NULL, 10 ) : 0;
#endif
                const char * pszType = pRow[1] ? pRow[1] : "";
                record.m_fIsChar = ( strcmpi( pszType, "char" ) == 0 );
                const char * pszSubtype = pRow[2] ? pRow[2] : "";
                record.m_iBaseId = (int) strtol( pszSubtype, NULL, 0 );
                record.m_sSerialized = pRow[3] ? pRow[3] : "";
                objects.push_back( record );
        }

        return true;
}

bool MySqlStorageService::LoadGMPages( std::vector<GMPageRecord> & pages )
{
        pages.clear();
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureGMPageColumns())
        {
                return false;
        }

        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        CGString sQuery;
        sQuery.Format( "SELECT `account_id`,`account_name`,`reason`,`page_time`,`pos_x`,`pos_y`,`pos_z`,`map_plane` FROM `%s` ORDER BY `id`;",
                (const char *) sGMPages );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result || !result->IsValid())
        {
                return true;
        }

        Storage::IDatabaseResult::Row pRow;
        while (( pRow = result->FetchRow()) != NULL )
        {
                GMPageRecord record;
                unsigned int accountId = pRow[0] ? (unsigned int) strtoul( pRow[0], NULL, 10 ) : 0;
                record.m_sAccount = pRow[1] ? pRow[1] : "";
                if ( record.m_sAccount.IsEmpty() && accountId > 0 )
                {
                        record.m_sAccount = GetAccountNameById( accountId );
                }
                record.m_sReason = pRow[2] ? pRow[2] : "";
#ifdef _WIN32
                record.m_lTime = pRow[3] ? (long) _strtoi64( pRow[3], NULL, 10 ) : 0;
#else
                record.m_lTime = pRow[3] ? (long) strtoll( pRow[3], NULL, 10 ) : 0;
#endif
                record.m_iPosX = pRow[4] ? atoi( pRow[4] ) : 0;
                record.m_iPosY = pRow[5] ? atoi( pRow[5] ) : 0;
                record.m_iPosZ = pRow[6] ? atoi( pRow[6] ) : 0;
                record.m_iMapPlane = pRow[7] ? atoi( pRow[7] ) : 0;
                pages.push_back( record );
        }
        return true;
}

bool MySqlStorageService::LoadServers( std::vector<ServerRecord> & servers )
{
        servers.clear();
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureServerColumns())
        {
                return false;
        }

        const CGString sServers = GetPrefixedTableName( "servers" );
        CGString sQuery;
        sQuery.Format( "SELECT `name`,`address`,`port`,`status`,`time_zone`,`clients_avg`,`url`,`email`,`register_password`,`notes`,`language`,`version`,`acc_app`,`last_valid_seconds`,`age_hours` FROM `%s` ORDER BY `name`;",
                (const char *) sServers );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result || !result->IsValid())
        {
                return true;
        }

        Storage::IDatabaseResult::Row pRow;
        while (( pRow = result->FetchRow()) != NULL )
        {
                ServerRecord record;
                record.m_sName = pRow[0] ? pRow[0] : "";
                record.m_sAddress = pRow[1] ? pRow[1] : "";
                record.m_iPort = pRow[2] ? atoi( pRow[2] ) : 0;
                record.m_iStatus = pRow[3] ? atoi( pRow[3] ) : 0;
                record.m_iTimeZone = pRow[4] ? atoi( pRow[4] ) : 0;
                record.m_iClientsAvg = pRow[5] ? atoi( pRow[5] ) : 0;
                record.m_sURL = pRow[6] ? pRow[6] : "";
                record.m_sEmail = pRow[7] ? pRow[7] : "";
                record.m_sRegisterPassword = pRow[8] ? pRow[8] : "";
                record.m_sNotes = pRow[9] ? pRow[9] : "";
                record.m_sLanguage = pRow[10] ? pRow[10] : "";
                record.m_sVersion = pRow[11] ? pRow[11] : "";
                record.m_iAccApp = pRow[12] ? atoi( pRow[12] ) : 0;
                record.m_iLastValidSeconds = pRow[13] ? atoi( pRow[13] ) : 0;
                record.m_iAgeHours = pRow[14] ? atoi( pRow[14] ) : 0;
                servers.push_back( record );
        }
        return true;
}

CGString MySqlStorageService::GetAccountNameById( unsigned int accountId )
{
        if ( accountId == 0 )
        {
                return CGString();
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sQuery;
        sQuery.Format( "SELECT `name` FROM `%s` WHERE `id` = %u LIMIT 1;", (const char *) sAccounts, accountId );

        std::unique_ptr<Storage::IDatabaseResult> result;
        if ( ! Query( sQuery, &result ))
        {
                return CGString();
        }

        CGString sName;
        if ( result && result->IsValid())
        {
                Storage::IDatabaseResult::Row pRow = result->FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        sName = pRow[0];
                }
        }

        return sName;
}

bool MySqlStorageService::SaveWorldObjectInternal( CObjBase * pObject )
{
        if ( pObject == NULL )
        {
                return false;
        }

        CGString sSerialized;
        if ( ! SerializeWorldObject( pObject, sSerialized ))
        {
                return false;
        }

        if ( ! UpsertWorldObjectMeta( pObject, sSerialized ))
        {
                return false;
        }
        if ( ! UpsertWorldObjectData( pObject, sSerialized ))
        {
                return false;
        }
        if ( ! RefreshWorldObjectComponents( pObject ))
        {
                return false;
        }
        if ( ! RefreshWorldObjectRelations( pObject ))
        {
                return false;
        }

        return true;
}

bool MySqlStorageService::SerializeWorldObject( CObjBase * pObject, CGString & outSerialized ) const
{
        if ( pObject == NULL )
        {
                return false;
        }

        std::string sTempPath;
        if ( ! GenerateTempScriptPath( sTempPath ))
        {
                return false;
        }

        TempFileGuard tempFile( sTempPath );
        if ( ! tempFile.IsValid())
        {
                return false;
        }

        CScript script;
        if ( ! script.Open( tempFile.GetPath().c_str(), OF_WRITE | OF_TEXT ))
        {
                return false;
        }

        pObject->r_Write( script );
        script.Close();

        std::ifstream input( tempFile.GetPath().c_str(), std::ios::in | std::ios::binary );
        if ( ! input.is_open())
        {
                return false;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        input.close();

        const std::string serialized = buffer.str();
        outSerialized = serialized.c_str();

        return true;
}

bool MySqlStorageService::ApplyWorldObjectData( CObjBase & object, const CGString & serialized ) const
{
        std::string sTempPath;
        if ( ! GenerateTempScriptPath( sTempPath ))
        {
                return false;
        }

        TempFileGuard tempFile( sTempPath );
        if ( ! tempFile.IsValid())
        {
                return false;
        }

        {
                std::ofstream output( tempFile.GetPath().c_str(), std::ios::out | std::ios::binary );
                if ( ! output.is_open())
                {
                        return false;
                }
                output << (const char *) serialized;
        }

        CScript script;
        if ( ! script.Open( tempFile.GetPath().c_str(), OF_READ | OF_TEXT ))
        {
                return false;
        }

        bool fResult = false;
        if ( script.FindNextSection())
        {
                fResult = object.r_Load( script );
        }

        script.Close();
        return fResult;
}

bool MySqlStorageService::UpsertWorldObjectMeta( CObjBase * pObject, const CGString & serialized )
{
        (void) serialized;

        if ( pObject == NULL )
        {
                return false;
        }

        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
        Storage::Repository::WorldObjectMetaRecord record;
        record.m_Uid = uid;
        record.m_ObjectType = pObject->IsChar() ? "char" : "item";

        CGString sSubtype;
        sSubtype.Format( "0x%x", (unsigned int) pObject->GetBaseID());
        record.m_ObjectSubtype = (const char *) sSubtype;

        const TCHAR * pszName = pObject->GetName();
        if ( pszName != NULL && pszName[0] != '\0' )
        {
                record.m_HasName = true;
                record.m_Name = pszName;
        }

        if ( pObject->IsChar())
        {
                CChar * pChar = dynamic_cast<CChar*>( pObject );
                if ( pChar != NULL && pChar->m_pPlayer != NULL )
                {
                        CAccount * pAccount = pChar->m_pPlayer->GetAccount();
                        if ( pAccount != NULL )
                        {
                                CGString sAccountName = CGString( pAccount->GetName());
                                if ( ! sAccountName.IsEmpty())
                                {
                                        unsigned int accountId = GetAccountId( sAccountName );
                                        if ( accountId > 0 )
                                        {
                                                record.m_HasAccountId = true;
                                                record.m_AccountId = accountId;
                                        }
                                }
                        }
                }
        }

        if ( pObject->IsItem())
        {
                const CItem * pItem = dynamic_cast<const CItem*>( pObject );
                if ( pItem != NULL )
                {
                        const CObjBase * pContainer = pItem->GetContainer();
                        if ( pContainer != NULL )
                        {
                                record.m_HasContainerUid = true;
                                record.m_ContainerUid = (unsigned long long) (UINT) pContainer->GetUID();
                        }
                }
        }

        CObjBaseTemplate * pTop = pObject->GetTopLevelObj();
        if ( pTop != NULL )
        {
                CObjBase * pTopObj = dynamic_cast<CObjBase*>( pTop );
                if ( pTopObj != NULL )
                {
                        record.m_HasTopLevelUid = true;
                        record.m_TopLevelUid = (unsigned long long) (UINT) pTopObj->GetUID();
                }
        }

        if ( pObject->IsTopLevel())
        {
                const CPointMap & pt = pObject->GetTopPoint();
                record.m_HasPosX = true;
                record.m_HasPosY = true;
                record.m_HasPosZ = true;
                record.m_PosX = pt.m_x;
                record.m_PosY = pt.m_y;
                record.m_PosZ = pt.m_z;
        }
        else if ( pObject->IsItem() && pObject->IsInContainer())
        {
                const CItem * pItem = dynamic_cast<const CItem*>( pObject );
                if ( pItem != NULL )
                {
                        const CPointMap & pt = pItem->GetContainedPoint();
                        record.m_HasPosX = true;
                        record.m_HasPosY = true;
                        record.m_HasPosZ = true;
                        record.m_PosX = pt.m_x;
                        record.m_PosY = pt.m_y;
                        record.m_PosZ = pt.m_z;
                }
        }

        const CGString sWorldObjects = GetPrefixedTableName( "world_objects" );
        Storage::Repository::WorldObjectMetaRepository repository( *this, sWorldObjects );
        return repository.Upsert( record );
}

bool MySqlStorageService::UpsertWorldObjectData( const CObjBase * pObject, const CGString & serialized )
{
        if ( pObject == NULL )
        {
                return false;
        }

        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
        Storage::Repository::WorldObjectDataRecord record;
        record.m_ObjectUid = uid;
        record.m_Data = (const char *) serialized;

        CGString sChecksum = ComputeSerializedChecksum( serialized );
        if ( ! sChecksum.IsEmpty())
        {
                record.m_HasChecksum = true;
                record.m_Checksum = (const char *) sChecksum;
        }

        const CGString sWorldObjectData = GetPrefixedTableName( "world_object_data" );
        Storage::Repository::WorldObjectDataRepository repository( *this, sWorldObjectData );
        return repository.Upsert( record );
}

bool MySqlStorageService::RefreshWorldObjectComponents( const CObjBase * pObject )
{
        if ( pObject == NULL )
        {
                return false;
        }

        const CGString sComponents = GetPrefixedTableName( "world_object_components" );
        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();

        Storage::Repository::WorldObjectComponentRepository repository( *this, sComponents );
        if ( !repository.DeleteForObject( uid ))
        {
                return false;
        }

        const CVarDefMap * pTagMap = pObject->GetTagDefs();
        const CVarDefMap * pVarMap = pObject->GetBaseDefs();

        std::vector<Storage::Repository::WorldObjectComponentRecord> records;
        records.reserve( ( pTagMap ? pTagMap->GetCount() : 0 ) +
                ( pVarMap ? pVarMap->GetCount() : 0 ));

        auto appendMap = [&]( const CVarDefMap * pMap, const char * component )
        {
                if ( pMap == NULL || component == NULL || component[0] == '\0' )
                {
                        return;
                }

                const size_t count = pMap->GetCount();
                for ( size_t i = 0; i < count; ++i )
                {
                        const CVarDefCont * pVar = pMap->GetAt( i );
                        if ( pVar == NULL )
                        {
                                continue;
                        }

                        Storage::Repository::WorldObjectComponentRecord record;
                        record.m_ObjectUid = uid;
                        record.m_Component = component;

                        const TCHAR * pszKey = pVar->GetKey();
                        if ( pszKey != NULL )
                        {
                                record.m_Name = pszKey;
                        }
                        else
                        {
                                record.m_Name.clear();
                        }

                        record.m_Sequence = static_cast<int>( i );

                        const TCHAR * pszVal = pVar->GetValStr();
                        if ( pszVal != NULL && pszVal[0] != '\0' )
                        {
                                record.m_HasValue = true;
                                record.m_Value = pszVal;
                        }

                        records.push_back( std::move( record ));
                }
        };

        appendMap( pTagMap, "TAG" );
        appendMap( pVarMap, "VAR" );

        if ( records.empty())
        {
                return true;
        }

        return repository.InsertMany( records );
}

bool MySqlStorageService::RefreshWorldObjectRelations( const CObjBase * pObject )
{
        if ( pObject == NULL )
        {
                return false;
        }

        const CGString sRelations = GetPrefixedTableName( "world_object_relations" );
        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();

        Storage::Repository::WorldObjectRelationRepository repository( *this, sRelations );
        if ( !repository.DeleteForObject( uid ))
        {
                return false;
        }

        std::vector<Storage::Repository::WorldObjectRelationRecord> records;

        if ( pObject->IsItem())
        {
                const CItem * pItem = dynamic_cast<const CItem*>( pObject );
                if ( pItem != NULL )
                {
                        const CObjBase * pContainer = pItem->GetContainer();
                        if ( pContainer != NULL )
                        {
                                Storage::Repository::WorldObjectRelationRecord record;
                                record.m_ParentUid = (unsigned long long) (UINT) pContainer->GetUID();
                                record.m_ChildUid = uid;
                                record.m_Relation = pItem->IsEquipped() ? "equipped" : "container";
                                record.m_Sequence = 0;
                                records.push_back( std::move( record ));
                        }
                }
        }

        if ( records.empty())
        {
                return true;
        }

        return repository.InsertMany( records );
}

#endif // !UNIT_TEST || UNIT_TEST_MYSQL_IMPLEMENTATION

bool MySqlStorageService::EnsureSchemaVersionTable()
{
        return m_SchemaManager.EnsureSchemaVersionTable(*this);
}

int MySqlStorageService::GetSchemaVersion()
{
        return m_SchemaManager.GetSchemaVersion(*this);
}

bool MySqlStorageService::SetSchemaVersion(int version)
{
        return m_SchemaManager.SetSchemaVersion(*this, version);
}

bool MySqlStorageService::ApplyMigration_0_1()
{
        return m_SchemaManager.ApplyMigration_0_1(*this);
}

bool MySqlStorageService::ApplyMigration_1_2()
{
        return m_SchemaManager.ApplyMigration_1_2(*this);
}

bool MySqlStorageService::ApplyMigration_2_3()
{
        return m_SchemaManager.ApplyMigration_2_3(*this);
}

bool MySqlStorageService::EnsureColumnExists(const CGString & table, const char * column, const char * definition)
{
        return m_SchemaManager.EnsureColumnExists(*this, table, column, definition);
}

bool MySqlStorageService::ColumnExists(const CGString & table, const char * column) const
{
        return m_SchemaManager.ColumnExists( const_cast<MySqlStorageService &>(*this), table, column );
}

bool MySqlStorageService::InsertOrUpdateSchemaValue(int id, int value)
{
        return m_SchemaManager.InsertOrUpdateSchemaValue(*this, id, value);
}

bool MySqlStorageService::QuerySchemaValue(int id, int & value)
{
        return m_SchemaManager.QuerySchemaValue(*this, id, value);
}

bool MySqlStorageService::EnsureSectorColumns()
{
        return m_SchemaManager.EnsureSectorColumns(*this);
}

bool MySqlStorageService::EnsureGMPageColumns()
{
        return m_SchemaManager.EnsureGMPageColumns(*this);
}

bool MySqlStorageService::EnsureServerColumns()
{
        return m_SchemaManager.EnsureServerColumns(*this);
}

bool MySqlStorageService::ApplyMigration(int fromVersion)
{
        return m_SchemaManager.ApplyMigration(*this, fromVersion);
}

bool MySqlStorageService::EnsureSchema()
{
        return m_SchemaManager.EnsureSchema(*this);
}

bool MySqlStorageService::IsLegacyImportCompleted()
{
        return m_SchemaManager.IsLegacyImportCompleted(*this);
}

bool MySqlStorageService::SetLegacyImportCompleted()
{
        return m_SchemaManager.SetLegacyImportCompleted(*this);
}

bool MySqlStorageService::SetWorldSaveCount(int saveCount)
{
        return m_SchemaManager.SetWorldSaveCount(*this, saveCount);
}

bool MySqlStorageService::GetWorldSaveCount(int & saveCount)
{
        return m_SchemaManager.GetWorldSaveCount(*this, saveCount);
}

bool MySqlStorageService::SetWorldSaveCompleted(bool fCompleted)
{
        return m_SchemaManager.SetWorldSaveCompleted(*this, fCompleted);
}

bool MySqlStorageService::GetWorldSaveCompleted(bool & fCompleted)
{
        return m_SchemaManager.GetWorldSaveCompleted(*this, fCompleted);
}

bool MySqlStorageService::LoadWorldMetadata(int & saveCount, bool & fCompleted)
{
        return m_SchemaManager.LoadWorldMetadata(*this, saveCount, fCompleted);
}

