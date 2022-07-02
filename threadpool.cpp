#include "threadpool.hpp"
#include <functional>

const int TASK_MAX_THRESHOLD = 1024;

/// 线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , taskSize_(0)
    , taskQueMaxSizeThreshold(TASK_MAX_THRESHOLD)
    , poolMode(PoolMode::MODE_FIXED){
};

/// 线程池析构
ThreadPool::~ThreadPool() {

}



/// 设置线程池模式
void ThreadPool::setMode(PoolMode mode) {
    poolMode = mode;
}

/// 设置任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshold) {
    taskQueMaxSizeThreshold = threshold;
}

/// 设置初始的线程数量
void ThreadPool::setInitThreadSize(int size) {
    initThreadSize_ = size;
}

/// 开启线程池
void ThreadPool::start(int initThreadSize) {
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;

    // 创建线程对象
    // 集中创建再启动，更加公平
    for (int i = 0; i < initThreadSize_; ++i) {
        // 创建线程对象的时候把线程函数给到 thread 线程对象
        threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));

    }

    // 启动所有线程 std::vector<Thread*> thread_;
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();   //去执行一个线程函数
    }
};

/// 给线程池提交任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp) {

}

/// 定义线程函数
void ThreadPool::threadFunc() {

}


//////////////////////// 线程方法实现

/// 启动线程
void Thread::start() {

}