// simple_thread.h
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>
#include <utility>
#include <assert.h>

#define DCHECK_RUN_ON(t) assert( SimpleThread::Current() == t );

class SimpleThread {
public:
    SimpleThread();
    explicit SimpleThread(const std::string& name);
    ~SimpleThread();

    // 禁止拷贝
    SimpleThread(const SimpleThread&) = delete;
    SimpleThread& operator=(const SimpleThread&) = delete;

    // 启动线程（开始消息循环）
    void Start();

    // 停止线程（退出消息循环，并等待线程结束）
    void Stop();

    // 异步投递任务（立即执行）
    void PostTask(std::function<void()> task);

    // 异步投递延迟任务（单位：毫秒）
    void PostDelayedTask(std::function<void()> task, int delay_ms);

    // 同步调用任务（阻塞直至执行完毕，返回结果）
    template<typename F, typename... Args>
    auto Invoke(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> result = task->get_future();
        PostTask([task]() { (*task)(); });
        return result;  // 调用者可以 get() 阻塞等待
    }

    // 检查当前线程是否是该 SimpleThread 实例
    bool IsCurrent() const;

    // 获取当前线程的 SimpleThread 实例（若无则返回 nullptr）
    static SimpleThread* Current();

private:
    // 内部任务节点（支持延迟）
    struct TaskNode {
        std::chrono::steady_clock::time_point when;
        std::function<void()> func;
        bool operator>(const TaskNode& other) const { return when > other.when; }
    };

    // 消息循环主函数
    void Run();

    // 处理一个任务（阻塞直到有任务可执行）
    void ProcessTasks();

    std::string name_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> running_;
    std::atomic<bool> stop_flag_;

    // 任务队列（按时间戳排序，支持延迟）
    std::priority_queue<TaskNode, std::vector<TaskNode>, std::greater<TaskNode>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 当前线程的 thread_local 指针
    static thread_local SimpleThread* current_instance_;
};
