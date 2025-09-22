#include "Storage/DirtyQueue.h"

#include "../graysvr.h"

#include <vector>

namespace Storage
{
        DirtyQueue::DirtyQueue() :
                m_Stop( false ),
                m_Running( false )
        {
        }

        DirtyQueue::~DirtyQueue()
        {
                Stop();
        }

        void DirtyQueue::Start( ProcessCallback callback )
        {
                std::lock_guard<std::mutex> lock( m_Mutex );
                if ( m_Running )
                        return;
                m_Callback = std::move( callback );
                m_Stop = false;
                m_Running = true;
                m_Thread = std::thread( &DirtyQueue::WorkerLoop, this );
        }

        void DirtyQueue::Stop()
        {
                {
                        std::lock_guard<std::mutex> lock( m_Mutex );
                        if ( !m_Running )
                                return;
                        m_Stop = true;
                }
                m_Condition.notify_all();
                if ( m_Thread.joinable())
                {
                        m_Thread.join();
                }
                {
                        std::lock_guard<std::mutex> lock( m_Mutex );
                        m_Running = false;
                        m_Stop = false;
                        m_Callback = ProcessCallback();
                        m_Queue.clear();
                        m_Pending.clear();
                }
        }

        void DirtyQueue::Enqueue( unsigned long long uid, StorageDirtyType type )
        {
                std::unique_lock<std::mutex> lock( m_Mutex );
                if ( !m_Running || !m_Callback )
                        return;

                if ( type == StorageDirtyType_Delete )
                {
                        m_Pending.erase( uid );
                        return;
                }

                auto it = m_Pending.find( uid );
                if ( it == m_Pending.end())
                {
                        m_Pending.emplace( uid, StorageDirtyType_Save );
                        m_Queue.push_back( uid );
                        lock.unlock();
                        m_Condition.notify_one();
                        return;
                }

                if ( it->second != StorageDirtyType_Delete )
                {
                        it->second = StorageDirtyType_Save;
                }
        }

        void DirtyQueue::WorkerLoop()
        {
                std::unique_lock<std::mutex> lock( m_Mutex );
                while ( true )
                {
                        m_Condition.wait( lock, [this]()
                        {
                                return m_Stop || !m_Queue.empty();
                        });

                        if ( m_Stop && m_Queue.empty())
                                break;

                        std::vector<std::pair<unsigned long long, StorageDirtyType>> batch;
                        while ( !m_Queue.empty())
                        {
                                unsigned long long uid = m_Queue.front();
                                m_Queue.pop_front();
                                auto it = m_Pending.find( uid );
                                if ( it == m_Pending.end())
                                        continue;
                                batch.emplace_back( uid, it->second );
                                m_Pending.erase( it );
                        }

                        ProcessCallback callback = m_Callback;
                        lock.unlock();
                        for ( const auto & entry : batch )
                        {
                                if ( callback )
                                {
                                        callback( entry.first, entry.second );
                                }
                        }
                        lock.lock();
                }
        }
}

