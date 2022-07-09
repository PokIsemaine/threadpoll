#include "threadpool.hpp"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHOLD = INT32_MAX;
const int THREAD_MAX_THRESHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60; //seconds

/// 线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , taskSize_(0)
    , taskQueMaxSizeThreshold_(TASK_MAX_THRESHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
    , isPoolRunning_(false)
    , idleThreadSize_(0)
    , threadSizeThreshold_(THREAD_MAX_THRESHOLD)
    , curThreadSize_(0) {
}

/// 线程池析构 用户的线程（需要线程通信）
ThreadPool::~ThreadPool() {
    isPoolRunning_ = false;
    // 先都给唤醒了
    notEmpty_.notify_all();

    // 等待线程池所有的线程返回 两种状态：阻塞 & 正在执行任务中
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    // 等待容器里的线程对象都清空了才析构
    exitCond_.wait(lock,[&]()->bool {return threads_.size() == 0;});

}

/// 设置线程池模式
void ThreadPool::setMode(PoolMode mode) {
    if(checkRunningState()) return;
    poolMode_ = mode;
}

/// 设置任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshold) {
    if(checkRunningState()) return;
    taskQueMaxSizeThreshold_ = threshold;
}

/// 设置线程池 cached 模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshold) {
    if(checkRunningState()) return;
    if(poolMode_ == PoolMode::MODE_CACHED) {
        threadSizeThreshold_ = threshold;
    }
}

/// 设置初始的线程数量
void ThreadPool::setInitThreadSize(int size) {
    if(checkRunningState()) return;
    initThreadSize_ = size;
}

/// 开启线程池
void ThreadPool::start(int initThreadSize) {
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 设置线程池的运行状态
    isPoolRunning_ = true;
    // 创建线程对象, 集中创建再启动，更加公平
    for (int i = 0; i < initThreadSize_; ++i) {
        // 创建线程对象的时候把线程函数给到 thread 线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }

    // 启动所有线程
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();   // 去执行一个线程函数
        idleThreadSize_++;      // 记录初始空闲线程数量
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
    // 用户提交任务，最长不能阻塞超过 1s, 否则提交任务失败
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

    // cached模式 任务处理比较紧急 场景：小而快的任务
    // 需要根据任务数量和空闲线程数量，判断是否需要创建新的线程出来
    if(poolMode_ == PoolMode::MODE_CACHED
        && taskSize_ > idleThreadSize_
        &&  curThreadSize_ < threadSizeThreshold_) {
        std::cout << "create new thread..." << std::endl;
        // 创建新线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));

        // 启动线程
        threads_[threadId]->start();

        // 修改线程个数相关变量
        curThreadSize_++;
        idleThreadSize_++;
    }

    // 返回任务的 Result 对象
    // return task->getResult();

    return Result(sp);

}

/// 定义线程函数 线程池的所有线程从任务队列里 消费任务
void ThreadPool::threadFunc(int threadid) { // 线程函数返回，相应的线程也就结束了
    auto lastTime = std::chrono::high_resolution_clock().now();
    while(isPoolRunning_){
        std::shared_ptr<Task> task;
        {
            // 先取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout << "tid:"<< std::this_thread::get_id() << "尝试获取任务..." << std::endl;


                while(taskQue_.size() == 0) {   // 任务队列没任务，就等 1s 看看情况；有任务就直接出去消费
                    if (poolMode_ == PoolMode::MODE_CACHED) {
                        // cached 模式下，有可能已经创建了很多的线程，但是空闲时间超过 60s
                        // 超过 initThreadSize_ 数量的线程要进行回收
                        // 当前时间 - 上次线程执行的时间 > 60s
                        // 每 1s 中返回一次 怎么区分超时返回还是有任务待执行返回？

                        if (std::cv_status::timeout ==
                            notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                            // 1s 后还是没任务 条件变量超时返回 检查如果线程空闲了 60s 那就回收掉
                            auto now = std::chrono::high_resolution_clock().now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if (dur.count() >= THREAD_MAX_IDLE_TIME
                                && curThreadSize_ > initThreadSize_) {
                                // 开始回收当前线程
                                // 记录线程数量的相关变量的值修改
                                // 把线程对象从线程列表容器中回收 没有办法匹配 threadFunc 是哪一个 Thread 对象
                                // thread id => thread 对象 => 删除
                                threads_.erase(threadid);   // 不要 std::this_thread::get_id()
                                curThreadSize_--;
                                idleThreadSize_--;

                                std::cout << "threadid:" << std::this_thread::get_id() << "exit" << std::endl;
                                // 通知一下主线程
                                exitCond_.notify_all();
                                return;
                            }
                        }
                    } else {
                        notEmpty_.wait(lock);
                    }

                    // 因为线程池要结束被唤醒
                    if(!isPoolRunning_) {
                        // 反正要结束了也不用维护那些变量了
                        threads_.erase(threadid);
                        std::cout << "threadid:" << std::this_thread::get_id() << "exit" << std::endl;
                        return;
                    }
                }


            std::cout << "tid:"<< std::this_thread::get_id() << "获取任务成功..." << std::endl;
            // 取到任务减少空闲线程数量
            idleThreadSize_--;

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
        // 执行完任务空闲了
        idleThreadSize_++;
        // 更新线程执行完任务的时间
        lastTime = std::chrono::high_resolution_clock().now();

    }

    // 结束线程池的时候正在执行任务，回来发现 isPoolRunning_ == false,就跳到这里
    threads_.erase(threadid);
    std::cout << "threadid:" << std::this_thread::get_id() << "exit" << std::endl;
    // 通知一下主线程
    exitCond_.notify_all();
    return;
}

bool ThreadPool::checkRunningState() const {
    return isPoolRunning_;
}


//////////////////////// 线程方法实现

int Thread::generateId_ = 0;

/// 线程构造
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++)
{}

/// 线程析构
Thread::~Thread() {}

/// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_,threadId_);   //C++11 线程对象t 和线程函数func_

    //分离线程对象和执行的线程函数
    t.detach(); // 设置分离线程 = pthread_detach 防止孤儿线程
}

int Thread::getId() const {
    return threadId_;
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