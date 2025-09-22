#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
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
                bool WaitForBatch( Batch & batch, std::stop_token stopToken );

        private:
                void CollectBatch( Batch & batch );

                std::mutex m_Mutex;
                std::condition_variable_any m_Condition;
                std::deque<unsigned long long> m_Queue;
                std::unordered_map<unsigned long long, StorageDirtyType> m_Pending;
        };
}

