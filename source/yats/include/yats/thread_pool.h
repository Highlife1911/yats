#pragma once

#include <atomic>
#include <iostream>
#include <mutex>
#include <queue>

#include <yats/thread_safe_queue.h>

namespace yats
{

class thread_pool
{
public:
    /**
     * Constructs a new thread pool.
     * @param thread_count Number of threads to use
     */
    explicit thread_pool(const size_t thread_count)
        : m_is_cancellation_requested(false), m_is_shutdown_requested(false)
    {
        m_threads.reserve(thread_count);
        for (size_t i = 0; i < thread_count; i++)
        {
            m_threads.emplace_back(&thread_pool::thread_function, this);
        }
    }

    thread_pool(const thread_pool& other) = delete;
    thread_pool(thread_pool&& other) = delete;

    thread_pool& operator=(const thread_pool& other) = delete;
    thread_pool& operator=(thread_pool&& other) = delete;

    ~thread_pool()
    {
        terminate();
    }

    /**
     * Executes function_to_execute in an own thread as soon as a
     * thread of the pool is available.
     * @param function_to_execute void() function that is to be executed in
     * a thread
     */
    void execute(const std::function<void()> & function_to_execute)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(function_to_execute);
        }
        m_function_added.notify_one();
    }

    /**
     * Terminates the thread pool. The thread pool is terminated
     * as soon as all threads have processed their current task.
     * This functions returns after all threads have terminated.
     */
    void terminate()
    {
        m_is_cancellation_requested = true;
        m_function_added.notify_all();
        join();
    }

    /**
     * Waits for the thread pool to process all tasks and terminates
     * the threads afterwards.
     * This function returns as all threads have been terminated.
     */
    void wait()
    {
        m_is_shutdown_requested = true;
        m_function_added.notify_all();
        join();
    }

protected:
    std::vector<std::thread> m_threads;
    std::queue < std::function<void()>> m_queue;
    std::atomic_bool m_is_cancellation_requested;
    std::atomic_bool m_is_shutdown_requested;
    std::mutex m_mutex;
    std::condition_variable m_function_added;

    /**
     * Joins all threads of the thread pool.
     */
    void join()
    {
        for (auto & thread : m_threads)
        {
            thread.join();
        }

        m_threads.clear();
    }

    void thread_function()
    {
        while(!m_is_cancellation_requested)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_function_added.wait(lock, [this] {
                return !m_queue.empty() || m_is_cancellation_requested || m_is_shutdown_requested;
            });
            if (m_is_cancellation_requested)
            {
                break;
            }
            if (m_queue.empty() && m_is_shutdown_requested)
            {
                break;
            }

            const auto run = m_queue.front();
            m_queue.pop();
            lock.unlock();

            run();
        }
    }
};
}
