#pragma once

#include <thread>

#include <yats/condition.h>

namespace yats
{

/**
 * Manager class that holds an arbitrary amount of threads that are synchonized 
 * using an externally controlled condition variable.
 */
class thread_pool
{
public:
    thread_pool(condition& condition)
        : m_condition(condition)
    {
    }

    thread_pool(const thread_pool& other) = delete;
    thread_pool(thread_pool&& other) = delete;

    thread_pool& operator=(const thread_pool& other) = delete;
    thread_pool& operator=(thread_pool&& other) = delete;

    ~thread_pool()
    {
        m_condition.terminate();
        for (auto& thread : m_threads)
        {
            thread.join();
        }
    }

    /**
     * Executes a user thread and places it into a constraint group.
     */
    template <typename Callable>
    void execute(Callable thread_function, size_t constraint)
    {
        m_threads.emplace_back([this, thread_function, constraint]() mutable {
            while (auto guard = m_condition.wait(constraint))
            {
                thread_function();
            }
        });
    }

protected:
    std::vector<std::thread> m_threads;
    condition& m_condition;
};
}
