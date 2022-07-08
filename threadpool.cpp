#include "threadpool.hpp"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHOLD = 1024;

/// 线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , taskSize_(0)
    , taskQueMaxSizeThreshold_(TASK_MAX_THRESHOLD)
    , poolMode(PoolMode::MODE_FIXED){
}

/// 线程池析构
ThreadPool::~ThreadPool() {}

/// 设置线程池模式
void ThreadPool::setMode(PoolMode mode) {
    poolMode = mode;
}

/// 设置任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshold) {
    taskQueMaxSizeThreshold_ = threshold;
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
}

/// 给线程池提交任务 用户调用该接口，传入任务对象 生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    // 线程的通信 等待任务队列有空余
//    while(taskQue_.size() == taskQueMaxSizeThreshold_) {
//        notFull_.wait(lock);
//    }
//  下面是 C++11 的简化写法
//    notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxSizeThreshold_;});
    // 用户提交任务，最长不能阻塞超过 1s, 否则盘大u你提交任务失败，返回 wait_for wait_until
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),
                      [&]()->bool {return taskQue_.size() < taskQueMaxSizeThreshold_;})) {
        // 表示 notFull_ 等待 1s, 条件依然没满足
        std::cerr << "task queue is full, submit task fail." << std::endl;
        // return task->getResult(); Task Result 考虑清楚生命周期
        // 线程执行完 task, task 对象就被析构掉了，result 依赖task 所以也不行，用下面的
        // 提交失败还要设置返回值无效
        return Result(sp, false);
    }

    // 如果有空余，把任务放到任务队列中
    taskQue_.emplace(sp);
    ++taskSize_;

    // 因为新放了任务，任务队列肯定不空了，在 notEmpty_ 上进行通知, 赶快分配线程执行任务 （消费）
    notEmpty_.notify_all();

    // 返回任务的 Result 对象
    // return task->getResult();

    return Result(sp);

}

/// 定义线程函数 线程池的所有线程从任务队列里 消费任务
void ThreadPool::threadFunc() {
//    std::cout << "begin threadFunc " << std::this_thread::get_id()
//                << std::endl;
//    std::cout << "end threadFunc " << std::this_thread::get_id()
//                 << std::endl;
    for(;;) {
        std::shared_ptr<Task> task;
        {
            // 先取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout << "tid:"<< std::this_thread::get_id()
                      << "尝试获取任务..." << std::endl;

            // 等待 notEmpty_ 条件
            notEmpty_.wait(lock,[&]()->bool {return !taskQue_.empty(); });

            std::cout << "tid:"<< std::this_thread::get_id()
                      << "获取任务成功..." << std::endl;

            // 从任务队列中取一个任务出来
            task = taskQue_.front();
            taskQue_.pop();
            --taskSize_;

            // 如果依然有剩余任务，继续通知其他的线程执行任务
            if(!taskQue_.empty()) {
                notEmpty_.notify_all();
            }

            // 取出一个任务，进行通知,通知可以继续提交生产任务
            notFull_.notify_all();
        }   // 取完任务就释放锁了，执行任务的时候不应该拿着锁占时间

        // 当前线程负责执行这个任务
        if(task != nullptr) {
            //  task->run();
            // 执行完一个任务, 把任务的返回值 setVal 方法给到 Result
            // 封装一个方法
            task->exec();
        }
    }
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

//////////////////////// Result 方法实现

Result::Result(std::shared_ptr<Task> task, bool isValid)
    : isValid_(isValid)
    , task_(task)
{
    task_->setResult(this);
}

/// 用户调用
Any Result::get() {
    if(!isValid_) {
        return "";
    }
    sem_.wait();        // task 任务如果没有执行完，这里会阻塞用户的线程
    return std::move(any_);
}

///
void Result::setVal(Any any) {
    // 存储 task 的返回值
    this->any_ = std::move(any);
    // 已经获取的任务返回值，增加信号量资源
    sem_.post();
}


//////////////////////// Task 方法实现

Task::Task()
    : result_(nullptr)
{}

void Task::exec() {
    if(result_ != nullptr) {
        result_->setVal(run()); // 这里发生多态调用
    }
}

void Task::setResult(Result* res) {
    result_ = res;
}