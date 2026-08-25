// simple_thread.cpp
#include "simple_thread.h"
#include "compatible.h"

#include <iostream>

thread_local SimpleThread* SimpleThread::current_instance_ = nullptr;

SimpleThread::SimpleThread() : SimpleThread("UnnamedThread") {}
SimpleThread::SimpleThread(const std::string& name)
    : name_(name), running_(false), stop_flag_(false) {}

SimpleThread::~SimpleThread() {
    Stop();
}

void SimpleThread::Start() {
    if (running_.exchange(true)) return;  // 已经启动
    stop_flag_ = false;
    thread_ = std::make_unique<std::thread>(&SimpleThread::Run, this);
}

void SimpleThread::Stop() {
    if (!running_.exchange(false)) return; // 已经停止
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_flag_ = true;
    }
    queue_cv_.notify_all();  // 唤醒可能阻塞的线程
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    thread_.reset();
}

void SimpleThread::PostTask(std::function<void()> task) {
    PostDelayedTask(std::move(task), 0);
}

void SimpleThread::PostDelayedTask(std::function<void()> task, int delay_ms) {
    auto now = std::chrono::steady_clock::now();
    auto when = now + std::chrono::milliseconds(delay_ms);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push({when, std::move(task)});
    }
    queue_cv_.notify_one();
}

bool SimpleThread::IsCurrent() const {
    return Current() == this;
}

SimpleThread* SimpleThread::Current() {
    return current_instance_;
}

void SimpleThread::Run() {
    current_instance_ = this;  // 设置 thread_local
    std::cout << "Thread [" << name_ << "] started." << std::endl;

    while (running_ && !stop_flag_) {
        ProcessTasks();
    }

    // 清除 thread_local（防止析构后误用）
    if (current_instance_ == this) {
        current_instance_ = nullptr;
    }
    std::cout << "Thread [" << name_ << "] stopped." << std::endl;
}

void SimpleThread::ProcessTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (task_queue_.empty()) {
        // 无任务，等待唤醒或停止
        queue_cv_.wait(lock, [this] { return !task_queue_.empty() || stop_flag_; });
        if (stop_flag_) return;
    }

    // 取出最早的任务（按时间排序）
    auto now = std::chrono::steady_clock::now();
    auto& earliest = task_queue_.top();
    if (earliest.when > now) {
        // 还没到执行时间，等待到该时刻或被唤醒
        auto wait_duration = earliest.when - now;
        if (queue_cv_.wait_for(lock, wait_duration, [this] { return stop_flag_.load(); })) {
            return;  // 被停止
        }
        // 若被虚假唤醒，重新检查时间
        if (!task_queue_.empty() && task_queue_.top().when > std::chrono::steady_clock::now()) {
            return; // 还没到，重新等待（下一次循环会再次处理）
        }
    }

    // 取出任务并执行（在锁内取出，但为了不长时间持有锁，复制或移动出来）
    auto task = std::move(const_cast<TaskNode&>(task_queue_.top()).func);
    task_queue_.pop();
    lock.unlock();  // 释放锁，执行任务
    task();
}