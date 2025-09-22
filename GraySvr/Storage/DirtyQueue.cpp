#include "Storage/DirtyQueue.h"

#include "../graysvr.h"

#include <vector>

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

bool DirtyQueue::WaitForBatch( Batch & batch, std::stop_token stopToken )
{
                std::unique_lock<std::mutex> lock( m_Mutex );
                const bool ready = m_Condition.wait( lock, stopToken, [this]()
                {
                        return !m_Queue.empty();
                });

                if ( !ready && m_Queue.empty())
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
}

