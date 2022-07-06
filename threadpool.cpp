#include "threadpool.hpp"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHOLD = 1024;

/// 线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , taskSize_(0)
    , taskQueMaxSizeThreshold(TASK_MAX_THRESHOLD)
    , poolMode(PoolMode::MODE_FIXED){
}

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

//        unique 左值引用拷贝复制被 delete，但是我们可以 std::move 变成右值
//        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this));
//        threads_.emplace_back(ptr); 报错，改为下 1 行
//        threads_.emplace_back(std::move(ptr));

        threads_.emplace_back(std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this)));

    }

    // 启动所有线程 std::vector<Thread*> thread_;
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();   //去执行一个线程函数
    }
};

/// 给线程池提交任务 用户调用该接口，传入任务对象 生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp) {
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    // 线程的通信 等待任务队列有空余
//    while(taskQue_.size() == taskQueMaxSizeThreshold_) {
//        notFull_.wait(lock);
//    }
//  下面是 C++11 的简化写法
    notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxSizeThreshold_;});
    // 用户提交任务，最长不能阻塞超过 1s, 否则盘大u你提交任务失败，返回

    // 如果有空余，把任务放到任务队列中
    taskQue_.emplace(sp);
    ++taskSize_;

    // 因为新放了任务，任务队列肯定不空了，在 notEmpty_ 上进行通知, 赶快分配线程执行任务 （消费）
    notEmpty_.notify_all();
}

/// 定义线程函数 线程池的所有线程从任务队列里 消费任务
void ThreadPool::threadFunc() {
    std::cout << "begin threadFunc " << std::this_thread::get_id()
                << std::endl;
    std::cout << "end threadFunc " << std::this_thread::get_id()
                 << std::endl;
}


//////////////////////// 线程方法实现

/// 线程构造
Thread::Thread(ThreadFunc func) :func_(func){}

/// 线程析构
Thread::~Thread() {}

/// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_);   //C++11 线程对象t 和线程函数func_

    //分离线程对象和执行的线程函数
    t.detach(); // 设置分离线程 = pthread_detach 防止孤儿线程
}
