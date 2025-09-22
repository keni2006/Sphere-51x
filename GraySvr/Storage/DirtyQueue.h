#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

enum StorageDirtyType : int;

namespace Storage
{
        class DirtyQueue
        {
        public:
                using Batch = std::vector<std::pair<unsigned long long, StorageDirtyType>>;

                DirtyQueue();
                ~DirtyQueue();

                void Enqueue( unsigned long long uid, StorageDirtyType type );
                /**
                * \brief Waits until there is work available or cancellation is requested.
                *
                * @param batch Destination container that will be filled with the
                *        operations to be processed.
                * @param stopRequested A flag that becomes true when the caller wants
                *        to cancel the wait.  The method returns false when the flag is
                *        set and no work is pending.
                * @return True when \p batch contains work items, false when the wait
                *         was cancelled before any new entry arrived.
                */
                bool WaitForBatch( Batch & batch, const std::atomic_bool & stopRequested );

                /**
                * \brief Wakes every waiter so they can observe a cancellation flag.
                */
                void NotifyAll();

        private:
                void CollectBatch( Batch & batch );

                std::mutex m_Mutex;
                std::condition_variable_any m_Condition;
                std::deque<unsigned long long> m_Queue;
                std::unordered_map<unsigned long long, StorageDirtyType> m_Pending;
        };
}

