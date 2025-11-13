#include <iostream>
#include <thread>
#include <future>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <type_traits>
#include <utility>

using fun = std::function<void(void)>;
class ThreadPool
{
public:
    ThreadPool(int thr_num = 1) : _stop(false)
    {
        for (int i = 0; i < thr_num; ++i)
        {
            _threads.emplace_back(&ThreadPool::entry, this);
        }
    }
    // push 传入函数和参数，内部将传入的函数封装成一个异步任务(package_task)，并存入任务队列
    template <typename F, typename... Args>
    auto push(F &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        // 1、将传入的函数封装成一个异步任务(package_task)
        using return_type = decltype(func(args...));
        auto tmp_func = std::bind(std::forward<F>(func), std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<return_type()>>(tmp_func);
        std::future<return_type> fut = task->get_future();
        // 2、构造一个lambad匿名函数(捕获任务对象)，函数内执行任务对象
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 如果线程池已停止，拒绝提交任务
            if (_stop.load())
            {
                throw std::runtime_error("push on stopped ThreadPool");
            }
            // 3、将构造出来的匿名函数对象，加入到任务池中
            _taskpool.push_back([task]()
                                { (*task)(); });
        }
        _condition.notify_one();
        return fut;
    }

    void stop()
    {
        // make stop() idempotent and safe to call multiple times
        bool expected = false;
        if (!_stop.compare_exchange_strong(expected, true))
            return;

        _condition.notify_all();
        for (auto &thr : _threads)
        {
            if (thr.joinable())
                thr.join();
        }
    }
    ~ThreadPool()
    {
        stop();
    }

private:
    // 线程入口函数，内部不断从任务队列中取出任务并执行
    void entry()
    {
        while (true)
        {
            std::vector<fun> tmp_taskpool;
            {
                // 加锁
                std::unique_lock<std::mutex> lock(_mutex);
                // 任务池不为空 或者_stop被置位
                _condition.wait(lock, [this]()
                                { return !_taskpool.empty() || _stop.load(); });

                // 如果停止且任务队列为空，退出线程
                if (_stop.load() && _taskpool.empty())
                    return;

                tmp_taskpool.swap(_taskpool);
            }
            // 取出任务进行执行
            for (auto &task : tmp_taskpool)
            {
                task();
            }
        }
    }

private:
    std::atomic<bool> _stop;
    std::vector<fun> _taskpool;
    std::condition_variable _condition;
    std::mutex _mutex;
    std::vector<std::thread> _threads;
};

int add(int a, int b)
{
    return a + b;
}

int main()
{
    ThreadPool pool(4);
    std::vector<std::future<int>> results;

    // 先提交所有任务
    for (int i = 0; i < 10; i++)
    {
        results.push_back(pool.push(add, i, i + 1));
    }

    // 再获取所有结果
    for (size_t i = 0; i < results.size(); i++)
    {
        std::cout << "Result: " << results[i].get() << std::endl;
    }

    pool.stop();
    return 0;
}
