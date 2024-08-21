#include "../include/scheduler.h"

namespace filetrans
{
    ThreadPool::ThreadPool(size_t count)
    {
        m_threadcounts = count;
        m_threads.resize(count);
        for(int i=0;i<count;i++)
        {
            m_threads[i] = std::thread(&ThreadPool::run,this);
        }
        // printf("thread create success.\n");
    }
    ThreadPool::~ThreadPool()
    {
        m_cond.notify_all();
        for(int i=0;i<m_threadcounts;i++)
        {
            if(m_threads[i].joinable())
            {
                printf("thread[%d] start .\n",i);
                m_threads[i].join();
            }
        }
    }
    void ThreadPool::add_task(Task* task)
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_tasks.push_back(task);
        m_task_counts++;
        lk.unlock();
        m_cond.notify_one();
    }
    void ThreadPool::run(void* arg)
    {
        ThreadPool* pool = static_cast<ThreadPool*>(arg);
        Task* task;
        while(true)
        {
            {
                std::unique_lock<std::mutex> lk(pool->m_mutex);
                while(pool->m_tasks.empty())
                {
                    pool->m_cond.wait(lk);
                }
                auto it = pool->m_tasks.begin();
                while(it!=pool->m_tasks.end())
                {
                    task = *it;
                    pool->m_tasks.erase(it++);
                    pool->m_active_thread_counts++;
                    break;
                }
                lk.unlock();
            }
            try
            {
                task->cb();
                pool->m_task_counts--;
                pool->m_active_thread_counts--;
            }
            catch(const std::exception& e)
            {
                std::cerr << "exception in threadpool task:"<<e.what() << '\n';
            }
            catch(...)
            {
                std::cerr << "unknown exception"<< '\n';
            }
        }
    }
}