#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

enum StorageDirtyType : int;

namespace Storage
{
        class DirtyQueue
        {
        public:
                using ProcessCallback = std::function<bool(unsigned long long, StorageDirtyType)>;

                DirtyQueue();
                ~DirtyQueue();

                void Start( ProcessCallback callback );
                void Stop();

                void Enqueue( unsigned long long uid, StorageDirtyType type );

        private:
                void WorkerLoop();

                ProcessCallback m_Callback;
                std::mutex m_Mutex;
                std::condition_variable m_Condition;
                std::deque<unsigned long long> m_Queue;
                std::unordered_map<unsigned long long, StorageDirtyType> m_Pending;
                std::thread m_Thread;
                bool m_Stop;
                bool m_Running;
        };
}

