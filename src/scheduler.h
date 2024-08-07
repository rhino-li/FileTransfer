#ifndef __FILE_SCHEDULER_H__
#define __FILE_SCHEDULER_H__

#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <list>
#include <iostream>
#include <condition_variable>

#include "mutex.h"

namespace filetrans
{
    struct Task
    {
        std::function<void()> cb;
        Task(std::function<void()> c):cb(std::move(c)){};
    };
   

    class ThreadPool : public std::enable_shared_from_this<ThreadPool>
    {
        private:
            std::vector<std::thread> m_threads;
            std::list<Task*> m_tasks;
            
            size_t m_threadcounts = 0; // 线程池总数
            size_t m_active_thread_counts = 0; // 活跃线程数
            size_t m_task_counts = 0;
            
            std::mutex m_mutex;
            std::condition_variable m_cond;
            
        public:
            typedef std::shared_ptr<ThreadPool> thread_pool_ptr;
            ThreadPool(size_t count=1);
            ~ThreadPool();
            void add_task(Task* task);
            static void run(void* arg);
    };

    

    
}

#endif