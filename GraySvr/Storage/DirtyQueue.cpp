#include "DirtyQueue.h"

#include "../graysvr.h"

namespace Storage
{
DirtyQueue::DirtyQueue()
{
}

DirtyQueue::~DirtyQueue()
{
}

void DirtyQueue::Enqueue( unsigned long long uid, StorageDirtyType type )
{
                std::unique_lock<std::mutex> lock( m_Mutex );
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

bool DirtyQueue::WaitForBatch( Batch & batch, const std::atomic_bool & stopRequested )
{
                std::unique_lock<std::mutex> lock( m_Mutex );
                while ( m_Queue.empty() && !stopRequested.load( std::memory_order_acquire ))
                {
                        m_Condition.wait( lock );
                }

                if ( m_Queue.empty())
                {
                        return false;
                }

                CollectBatch( batch );
                return true;
}

void DirtyQueue::CollectBatch( Batch & batch )
{
                batch.clear();
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
}

void DirtyQueue::NotifyAll()
{
                std::lock_guard<std::mutex> lock( m_Mutex );
                m_Condition.notify_all();
}
}

