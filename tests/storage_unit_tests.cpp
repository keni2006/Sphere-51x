#include "mysql_stub.h"
#include "stubs/graysvr.h"
#include "test_harness.h"

#include "Storage/DirtyQueue.h"
#include "Storage/MySql/ConnectionManager.h"
#include "Storage/MySql/MySqlConnection.h"

#include <stdexcept>
#include <stop_token>
#include <string>

TEST_CASE( TestConnectionManagerParsesCharsetAndCollation )
{
        g_Log.Clear();

        Storage::MySql::ConnectionManager manager;
        Storage::MySql::ConnectionManager::ConnectionDetails details;

        CServerMySQLConfig config;
        config.m_fEnable = true;
        config.m_sDatabase = "spheretest";
        config.m_sUser = "root";
        config.m_sPassword = "";
        config.m_sCharset = "latin1 utf8mb4_general_ci";

        if ( !manager.Connect( config, details ))
        {
                throw std::runtime_error( "ConnectionManager::Connect returned false" );
        }

        if ( details.m_TableCharset != "utf8mb4" )
        {
                throw std::runtime_error( "Derived table charset did not honor collation" );
        }
        if ( details.m_TableCollation != "utf8mb4_general_ci" )
        {
                throw std::runtime_error( "Connection manager did not preserve requested collation" );
        }

        bool mismatchLogged = false;
        for ( const auto & entry : g_Log.Events())
        {
                if ( entry.m_message.find( "does not match collation" ) != std::string::npos )
                {
                        mismatchLogged = true;
                        break;
                }
        }

        if ( !mismatchLogged )
        {
                throw std::runtime_error( "Charset mismatch warning was not emitted" );
        }

        manager.Disconnect();
}

TEST_CASE( TestMySqlConnectionPoolReusesConnections )
{
        Storage::DatabaseConfig config;
        config.m_Enable = true;
        config.m_Host = "localhost";
        config.m_Database = "spheretest";
        config.m_Username = "root";
        config.m_Password = "";
        config.m_Charset = "utf8mb4";
        config.m_Collation = "";

        Storage::MySql::MySqlConnectionPool pool( config, 1 );

        Storage::MySql::MySqlConnection * first = nullptr;
        {
                auto scoped = pool.Acquire();
                if ( !scoped.IsValid())
                {
                        throw std::runtime_error( "First connection acquisition failed" );
                }
                first = &scoped.Get();
                if ( !first->IsOpen())
                {
                        throw std::runtime_error( "Acquired connection was not open" );
                }
        }

        {
                auto scoped = pool.Acquire();
                if ( !scoped.IsValid())
                {
                        throw std::runtime_error( "Second connection acquisition failed" );
                }
                if ( &scoped.Get() != first )
                {
                        throw std::runtime_error( "Connection pool did not reuse idle connection" );
                }
        }

        pool.Shutdown();
}

TEST_CASE( TestDirtyQueueAggregatesAndRespectsCancellation )
{
        Storage::DirtyQueue queue;
        Storage::DirtyQueue::Batch batch;
        std::stop_source stopSource;

        queue.Enqueue( 0x100u, StorageDirtyType_Save );
        queue.Enqueue( 0x200u, StorageDirtyType_Save );
        queue.Enqueue( 0x100u, StorageDirtyType_Save );

        if ( !queue.WaitForBatch( batch, stopSource.get_token()))
        {
                throw std::runtime_error( "Dirty queue did not produce batch" );
        }
        if ( batch.size() != 2 )
        {
                throw std::runtime_error( "Dirty queue failed to coalesce duplicate save entries" );
        }

        queue.Enqueue( 0x200u, StorageDirtyType_Delete );
        queue.Enqueue( 0x200u, StorageDirtyType_Save );

        batch.clear();
        if ( !queue.WaitForBatch( batch, stopSource.get_token()))
        {
                throw std::runtime_error( "Dirty queue did not return batch after delete/save" );
        }
        if ( batch.size() != 1 || batch[0].first != 0x200u || batch[0].second != StorageDirtyType_Save )
        {
                throw std::runtime_error( "Dirty queue did not re-queue entry after delete" );
        }

        Storage::DirtyQueue idleQueue;
        Storage::DirtyQueue::Batch idleBatch;
        std::stop_source cancelled;
        cancelled.request_stop();
        if ( idleQueue.WaitForBatch( idleBatch, cancelled.get_token()))
        {
                throw std::runtime_error( "Dirty queue should have respected cancellation" );
        }
}
