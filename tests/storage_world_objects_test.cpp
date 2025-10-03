#include "storage_test_facade.h"
#include "mysql_stub.h"
#include "stubs/graysvr.h"
#include "test_harness.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
        std::string ComputeFnv1a64( const std::string & data )
        {
                unsigned long long hash = 1469598103934665603ULL;
                for ( unsigned char ch : data )
                {
                        hash ^= ch;
                        hash *= 1099511628211ULL;
                }

                std::ostringstream stream;
                stream << std::hex << std::nouppercase << std::setfill( '0' ) << std::setw( 16 ) << hash;
                return stream.str();
        }

        const ExecutedPreparedStatement * FindStatement( const std::vector<ExecutedPreparedStatement> & statements,
                const std::string & needle )
        {
                auto it = std::find_if( statements.rbegin(), statements.rend(), [&]( const ExecutedPreparedStatement & stmt )
                {
                        return stmt.query.find( needle ) != std::string::npos;
                });

                if ( it == statements.rend())
                {
                        return nullptr;
                }

                return &(*it);
        }

        class SkippingChar : public CChar
        {
        public:
                explicit SkippingChar( unsigned int skipCount = 1 ) : m_SkipWrites( skipCount ) {}

                void r_Write( CScript & script ) override
                {
                        if ( m_SkipWrites > 0 )
                        {
                                --m_SkipWrites;
                                return;
                        }

                        CObjBase::r_Write( script );
                }

        private:
                unsigned int m_SkipWrites;
        };
}

TEST_CASE( TestSaveWorldObjectPersistsMetaAndData )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();
        g_Log.Clear();

        CAccount account;
        account.SetName( "hero_account" );

        CPlayer player( &account );

        CChar character;
        character.SetUID( 0x01020304u );
        character.SetBaseID( 0x200 );
        character.SetName( "Hero" );
        character.SetPlayer( &player );
        character.SetTopLevel( true );
        character.SetTopPoint( CPointMap( 100, 200, 5 ));
        character.SetTopLevelObj( &character );

        CVarDefMap tags;
        tags.Add( CGString( "Strength" ), CGString( "90" ));
        CVarDefMap vars;
        vars.Add( CGString( "Dexterity" ), CGString( "80" ));
        character.SetTagDefs( &tags );
        character.SetBaseDefs( &vars );

        PushMysqlResultSet({ { "77" } });

        if ( !storage.Service().SaveWorldObject( &character ))
        {
                throw std::runtime_error( "SaveWorldObject returned false" );
        }

        const auto & statements = storage.ExecutedStatements();

        const ExecutedPreparedStatement * metaStmt = FindStatement( statements, "`test_world_objects`" );
        if ( metaStmt == nullptr )
        {
                throw std::runtime_error( "World object metadata statement was not executed" );
        }
        if ( metaStmt->parameters.size() != 10 )
        {
                throw std::runtime_error( "Unexpected parameter count for world object meta statement" );
        }

        if ( metaStmt->parameters[0] != "16909060" )
        {
                throw std::runtime_error( "UID parameter for world object meta was incorrect" );
        }
        if ( metaStmt->parameters[1] != "char" )
        {
                throw std::runtime_error( "Object type was not persisted as character" );
        }
        if ( metaStmt->parameters[2] != "0x200" )
        {
                throw std::runtime_error( "Object subtype did not match base id" );
        }
        if ( metaStmt->parameters[3] != "Hero" )
        {
                throw std::runtime_error( "Character name missing from metadata" );
        }
        if ( metaStmt->parameters[4] != "77" )
        {
                throw std::runtime_error( "Account id missing from metadata" );
        }
        if ( metaStmt->parameters[7] != "100" || metaStmt->parameters[8] != "200" || metaStmt->parameters[9] != "5" )
        {
                throw std::runtime_error( "Top level coordinates were not persisted" );
        }

        const ExecutedPreparedStatement * dataStmt = FindStatement( statements, "`test_world_object_data`" );
        if ( dataStmt == nullptr )
        {
                throw std::runtime_error( "World object data statement was not executed" );
        }
        if ( dataStmt->parameters.size() != 3 )
        {
                throw std::runtime_error( "Unexpected parameter count for world object data" );
        }
        if ( dataStmt->parameters[0] != "16909060" )
        {
                throw std::runtime_error( "UID parameter for world object data was incorrect" );
        }
        const std::string expectedData = "UID=16909060\n";
        if ( dataStmt->parameters[1] != expectedData )
        {
                throw std::runtime_error( "Serialized payload did not contain object data" );
        }
        if ( dataStmt->parameters[2] != ComputeFnv1a64( expectedData ))
        {
                throw std::runtime_error( "Checksum was not recorded for world object data" );
        }

        std::vector<const ExecutedPreparedStatement*> componentInserts;
        for ( const auto & stmt : statements )
        {
                if ( stmt.query.find( "`test_world_object_components`" ) != std::string::npos &&
                        stmt.query.find( "INSERT INTO" ) != std::string::npos )
                {
                        componentInserts.push_back( &stmt );
                }
        }

        if ( componentInserts.size() != 2 )
        {
                throw std::runtime_error( "Expected TAG and VAR component inserts" );
        }

        if ( componentInserts[0]->parameters[1] != "TAG" )
        {
                throw std::runtime_error( "TAG component record missing" );
        }
        if ( componentInserts[0]->parameters[2] != "Strength" )
        {
                throw std::runtime_error( "TAG component values were not captured" );
        }
        size_t tagValueIndex = componentInserts[0]->parameters.size() > 4 ? 4 : 3;
        if ( componentInserts[0]->parameters.size() <= tagValueIndex || componentInserts[0]->parameters[tagValueIndex] != "90" )
        {
                throw std::runtime_error( "TAG component values were not captured" );
        }

        if ( componentInserts[1]->parameters[1] != "VAR" )
        {
                throw std::runtime_error( "VAR component record missing" );
        }
        if ( componentInserts[1]->parameters[2] != "Dexterity" )
        {
                throw std::runtime_error( "VAR component values were not captured" );
        }
        size_t varValueIndex = componentInserts[1]->parameters.size() > 4 ? 4 : 3;
        if ( componentInserts[1]->parameters.size() <= varValueIndex || componentInserts[1]->parameters[varValueIndex] != "80" )
        {
                throw std::runtime_error( "VAR component values were not captured" );
        }
}

TEST_CASE( TestSaveWorldObjectPersistsAccountWhenMissing )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();
        g_Log.Clear();
        ClearMysqlResults();

        CAccount account;
        account.SetName( "fresh_account" );

        CPlayer player( &account );

        CChar character;
        character.SetUID( 0x01020304u );
        character.SetBaseID( 0x200 );
        character.SetName( "Hero" );
        character.SetPlayer( &player );
        character.SetTopLevel( true );
        character.SetTopPoint( CPointMap( 100, 200, 5 ));
        character.SetTopLevelObj( &character );

        PushMysqlResultSet({});
        PushMysqlResultSet({ { "88" } });
        PushMysqlResultSet({ { "88" } });

        if ( !storage.Service().SaveWorldObject( &character ))
        {
                throw std::runtime_error( "SaveWorldObject returned false" );
        }

        const auto & statements = storage.ExecutedStatements();

        const ExecutedPreparedStatement * metaStmt = FindStatement( statements, "`test_world_objects`" );
        if ( metaStmt == nullptr )
        {
                throw std::runtime_error( "World object metadata statement was not executed" );
        }
        if ( metaStmt->parameters.size() != 10 )
        {
                throw std::runtime_error( "Unexpected parameter count for world object meta statement" );
        }
        if ( metaStmt->parameters[4] != "88" )
        {
                throw std::runtime_error( "Account id was not persisted after inserting the account" );
        }

        const auto & queries = storage.ExecutedQueries();
        auto insertIt = std::find_if( queries.begin(), queries.end(), []( const std::string & query )
        {
                return query.find( "INSERT INTO" ) != std::string::npos &&
                        query.find( "`test_accounts`" ) != std::string::npos;
        });

        if ( insertIt == queries.end())
        {
                throw std::runtime_error( "Account insert query was not executed" );
        }
}

TEST_CASE( TestSaveItemPersistsContainerRelations )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        CItem container;
        container.SetUID( 0x02030405u );
        container.SetBaseID( 0x400 );
        container.SetName( "Chest" );
        container.SetTopLevel( true );
        container.SetTopPoint( CPointMap( 50, 60, 0 ));
        container.SetTopLevelObj( &container );

        storage.ResetQueryLog();
        if ( !storage.Service().SaveWorldObject( &container ))
        {
                throw std::runtime_error( "Failed to save container" );
        }

        CItem item;
        item.SetUID( 0x01000010u );
        item.SetBaseID( 0x401 );
        item.SetContainer( &container );
        item.SetTopLevel( false );
        item.SetInContainer( true );
        item.SetContainedPoint( CPointMap( 5, 6, 7 ));
        item.SetTopLevelObj( &container );

        storage.ResetQueryLog();
        if ( !storage.Service().SaveWorldObject( &item ))
        {
                throw std::runtime_error( "Failed to save contained item" );
        }

        const auto & statements = storage.ExecutedStatements();

        const ExecutedPreparedStatement * metaStmt = FindStatement( statements, "`test_world_objects`" );
        if ( metaStmt == nullptr )
        {
                throw std::runtime_error( "Contained item metadata was not persisted" );
        }
        if ( metaStmt->parameters[1] != "item" )
        {
                throw std::runtime_error( "Contained object type was not item" );
        }
        if ( metaStmt->parameters[5] != "33752069" )
        {
                throw std::runtime_error( "Container uid not captured in metadata" );
        }
        if ( metaStmt->parameters[6] != "33752069" )
        {
                throw std::runtime_error( "Top level uid for contained item incorrect" );
        }
        if ( metaStmt->parameters[7] != "5" || metaStmt->parameters[8] != "6" || metaStmt->parameters[9] != "7" )
        {
                throw std::runtime_error( "Contained item coordinates were not stored" );
        }

        const ExecutedPreparedStatement * relationStmt = nullptr;
        for ( const auto & stmt : statements )
        {
                if ( stmt.query.find( "`test_world_object_relations`" ) != std::string::npos &&
                        stmt.query.find( "INSERT INTO" ) != std::string::npos )
                {
                        relationStmt = &stmt;
                        break;
                }
        }
        if ( relationStmt == nullptr )
        {
                throw std::runtime_error( "World object relation insert missing" );
        }
        if ( relationStmt->parameters.size() < 3 )
        {
                throw std::runtime_error( "Unexpected relation parameter count" );
        }
        if ( relationStmt->parameters[0] != "33752069" || relationStmt->parameters[1] != "16777232" )
        {
                throw std::runtime_error( "Relation parent/child identifiers incorrect" );
        }
        if ( relationStmt->parameters[2] != "container" )
        {
                throw std::runtime_error( "Relation type not recorded as container" );
        }
        if ( relationStmt->parameters.size() >= 4 && relationStmt->parameters[3] != "0" )
        {
                throw std::runtime_error( "Relation sequence not recorded as zero" );
        }
}

TEST_CASE( TestSavingContainerDoesNotRemoveChildRelations )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        CItem container;
        container.SetUID( 0x02030405u );
        container.SetBaseID( 0x400 );
        container.SetTopLevel( true );
        container.SetTopPoint( CPointMap( 50, 60, 0 ));
        container.SetTopLevelObj( &container );

        storage.ResetQueryLog();
        if ( !storage.Service().SaveWorldObject( &container ))
        {
                throw std::runtime_error( "Failed to save container" );
        }

        CItem item;
        item.SetUID( 0x01000010u );
        item.SetBaseID( 0x401 );
        item.SetContainer( &container );
        item.SetTopLevel( false );
        item.SetInContainer( true );
        item.SetContainedPoint( CPointMap( 5, 6, 7 ));
        item.SetTopLevelObj( &container );

        storage.ResetQueryLog();
        if ( !storage.Service().SaveWorldObject( &item ))
        {
                throw std::runtime_error( "Failed to save contained item" );
        }

        storage.ResetQueryLog();
        if ( !storage.Service().SaveWorldObject( &container ))
        {
                throw std::runtime_error( "Failed to re-save container" );
        }

        const auto & statements = storage.ExecutedStatements();
        const ExecutedPreparedStatement * deleteStmt = nullptr;
        for ( const auto & stmt : statements )
        {
                if ( stmt.query.find( "`test_world_object_relations`" ) != std::string::npos &&
                        stmt.query.find( "DELETE FROM" ) != std::string::npos )
                {
                        deleteStmt = &stmt;
                        break;
                }
        }

        if ( deleteStmt == nullptr )
        {
                throw std::runtime_error( "Expected relation delete statement when saving container" );
        }
        if ( deleteStmt->query.find( "parent_uid" ) != std::string::npos )
        {
                throw std::runtime_error( "Relation delete unexpectedly targeted parent uid" );
        }
        if ( deleteStmt->parameters.size() != 1 )
        {
                throw std::runtime_error( "Relation delete should bind a single uid" );
        }
        if ( deleteStmt->parameters[0] != "33752069" )
        {
                throw std::runtime_error( "Relation delete bound incorrect uid" );
        }
}

TEST_CASE( TestCharacterSkippedSerializationPersistsBeforeItems )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();
        g_Log.Clear();
        ClearMysqlResults();

        g_World.m_fSaveParity = false;

        SkippingChar character;
        character.SetUID( 0x01020304u );
        character.SetBaseID( 0x200 );
        character.SetTopLevel( true );
        character.SetTopLevelObj( &character );

        CItem backpack;
        backpack.SetUID( 0x02030405u );
        backpack.SetBaseID( 0x401 );
        backpack.SetContainer( &character );
        backpack.SetTopLevel( false );
        backpack.SetInContainer( true );
        backpack.SetTopLevelObj( &character );

        std::vector<CObjBase*> objects;
        objects.push_back( &character );
        objects.push_back( &backpack );

        PushMysqlResultSet({});

        if ( !storage.Service().SaveWorldObjects( objects ))
        {
                throw std::runtime_error( "SaveWorldObjects returned false" );
        }

        const auto & statements = storage.ExecutedStatements();
        std::vector<const ExecutedPreparedStatement*> worldObjectUpserts;
        for ( const auto & stmt : statements )
        {
                if ( stmt.query.find( "`test_world_objects`" ) != std::string::npos &&
                        stmt.query.find( "INSERT INTO" ) != std::string::npos )
                {
                        worldObjectUpserts.push_back( &stmt );
                }
        }

        size_t charIndex = static_cast<size_t>( -1 );
        size_t itemIndex = static_cast<size_t>( -1 );
        for ( size_t i = 0; i < worldObjectUpserts.size(); ++i )
        {
                const auto & stmt = *worldObjectUpserts[i];
                if ( stmt.parameters.size() < 2 )
                {
                        continue;
                }

                const std::string & type = stmt.parameters[1];
                if ( type == "char" && charIndex == static_cast<size_t>( -1 ))
                {
                        charIndex = i;
                }
                else if ( type == "item" && itemIndex == static_cast<size_t>( -1 ))
                {
                        itemIndex = i;
                }
        }

        if ( charIndex == static_cast<size_t>( -1 ))
        {
                throw std::runtime_error( "Character metadata upsert missing" );
        }

        if ( itemIndex == static_cast<size_t>( -1 ))
        {
                throw std::runtime_error( "Item metadata upsert missing" );
        }

        if ( charIndex >= itemIndex )
        {
                throw std::runtime_error( "Item metadata upsert did not occur after character" );
        }
}

TEST_CASE( TestDeleteWorldObjectUsesPreparedStatement )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        CItem item;
        item.SetUID( 0x00ABCDEFu );
        item.SetBaseID( 0x500 );

        storage.ResetQueryLog();
        if ( !storage.Service().DeleteWorldObject( &item ))
        {
                throw std::runtime_error( "DeleteWorldObject returned false" );
        }

        const auto & statements = storage.ExecutedStatements();
        const ExecutedPreparedStatement * deleteStmt = FindStatement( statements, "DELETE FROM `test_world_objects`" );
        if ( deleteStmt == nullptr )
        {
                throw std::runtime_error( "World object delete did not issue a prepared statement" );
        }
        if ( deleteStmt->parameters.size() != 1 || deleteStmt->parameters[0] != "11259375" )
        {
                throw std::runtime_error( "World object delete bound incorrect uid" );
        }
}

TEST_CASE( TestLoadWorldObjectsIncludesAccountMetadata )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();
        g_Log.Clear();
        ClearMysqlResults();

        PushMysqlResultSet({
                { "16909060", "char", "0x200", "77", "hero_account", "UID=16909060\n" }
        });

        std::vector<MySqlStorageService::WorldObjectRecord> records;
        if ( !storage.Service().LoadWorldObjects( records ))
        {
                throw std::runtime_error( "LoadWorldObjects returned false" );
        }

        if ( records.size() != 1 )
        {
                throw std::runtime_error( "Expected a single world object record" );
        }

        const MySqlStorageService::WorldObjectRecord & record = records[0];
        if ( !record.m_fHasAccountId )
        {
                throw std::runtime_error( "World object record did not indicate an account id was present" );
        }
        if ( record.m_iAccountId != 77 )
        {
                throw std::runtime_error( "World object account id did not match result set" );
        }
        const std::string accountName = (const char *) record.m_sAccountName;
        if ( accountName != "hero_account" )
        {
                throw std::runtime_error( "World object account name did not match result set" );
        }
}
