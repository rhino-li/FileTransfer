/**
* 封装读写锁、信号量、互斥量和线程
*
**/

#ifndef __FILE_MUTEX_H__
#define __FILE_MUTEX_H__

#include <pthread.h>
#include <thread>
#include <semaphore.h>
#include <memory>
#include <functional>

namespace filetrans
{
    class Semaphore 
    {
        private:
            sem_t m_semaphore;
        public:
            Semaphore(uint32_t count=0);
            ~Semaphore();
            void wait();
            void notify();
    };
    class Mutex
    {
        private:
            pthread_mutex_t m_mutex;
        public:
            Mutex();
            ~Mutex();
            void lock();
            void unlock();
    };
    class RWMutex
    {
    private:
        pthread_rwlock_t m_lock;
    public:
        RWMutex() 
        {
            pthread_rwlock_init(&m_lock, nullptr);
        }
        ~RWMutex() 
        {
            pthread_rwlock_destroy(&m_lock);
        }
        void rdlock() // 读锁
        {
            pthread_rwlock_rdlock(&m_lock);
        }
        void wrlock() // 写锁
        {
            pthread_rwlock_wrlock(&m_lock);
        }
        void unlock() 
        {
            pthread_rwlock_unlock(&m_lock);
        }
    };
    // class Thread:public std::enable_shared_from_this<Thread>
    // {
    //     private:
    //         pid_t m_id=-1; // 线程id
    //         pthread_t m_thread = 0;
    //         std::function<void()> cb;
    //         Semaphore m_semaphore;
    //     public:
    //         typedef std::shared_ptr<Thread> thread_ptr;

    //         Thread(std::function<void()> cb);
    //         ~Thread();
    //         pid_t get_thread_id();
    //         void join();
    //         void cancel();
    //         void start();
    //         void run(void *arg);
    // };
    
}

#endif