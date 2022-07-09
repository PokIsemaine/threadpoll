#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <memory>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>


/**
 * @brief Any类型： 可以接收任意数据的类型
 * @note 任意的其他类型 temple
 *      能让一个类型 指向 其他任意的类型 ： 基类指向派生类类型
 *                        -----------------------
 *      Any =》 Base* --> * Derive:public Base  *
 *                        * template data       * <------- 把任意数据类型包含在 Derive 中
 *                        -----------------------
 */
class Any {
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    /// 这个构造函数可以让 Any 类型接收任意其他的数据
    template<typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data))
    {}

    /// 这个方法能把 Any 对象里面存储的 data 数据提取出来
    template<typename T>
    T cast_() {
        // 我们怎么从 base_ 找到它所指向的 Derive 对象，从它里面取出 data 成员变量
        // 基类指针转为派生类指针 dynamic_cast  基类类型指针要确实指向派生类对象 RTTI
        Derive<T> * pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr) { // 类型不匹配
            throw "type is unmatch!";
        }
        return pd->date_;
    }
private:

    class Base {
    public:
        virtual ~Base() = default;
    };

    template<typename T>
    class Derive : public Base {
    public:
        Derive(T data) : date_(data)
        {}
        /// 保存了任意其他类型
        T date_;
    };
private:
    /// 定义一个基类指针
    std::unique_ptr<Base> base_;
};


/**
 * @brief 实现一个信号量用于通信
 * @note
 *      提交任务的线程和z执行任务的线程不是一个，所以要线程通信(semaphore)
 *          submitTask线程            执行task线程
 *               |                      |
 *   ---<-- submitTask                  |
 *   |           |                      |
 * Result        |                      |
 *   |           |                      |
 *   ------->res.get 阻塞                |
 *               |   ^                  |
 *               |   |                  |
 *               |   ---semaphore-----task执行完
 */

class Semaphore {
public:
    Semaphore(int limit = 0)
        : resLimit_(limit)
    {}
    ~Semaphore() = default;

    /// 获取一个信号量资源
    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待信号量有资源，没有资源的话，会阻塞当前线程
        cond_.wait(lock,[&]()->bool {return resLimit_ > 0;});
        resLimit_--;
    }
    /// 增加一个信号量资源
    void post() {
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }
private:
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

class Task;

/**
 * @brief 实现接收提交到线程池的 task 任务执行完成后的返回值类型 Result
 */
class Result {
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    /// setVal 方法，获取任务执行完的返回值
    void setVal(Any any);

    /// get 方法，用户调用这个方法获取 task 的返回值
    Any get();
private:
    /// 存储任务的返回值
    Any any_;
    /// 线程通信信号量
    Semaphore sem_;
    /// 执行对应获的任务对象, 强智能指针，task 引用计数不为 0 不会析构
    std::shared_ptr<Task> task_;
    /// 返回值是否有效
    std::atomic_bool isValid_;
};

/**
 * @brief 任务抽象基类
 * 
 */
class Task {
public:
    Task();
    ~Task() = default;
    void exec();
    void setResult(Result* res);

    ///用户可以自定义任务数据类型，从 Task 继承重写 run 方法，实现自定义任务处理
	virtual Any run() = 0;
private:
    /// 如果强智能指针的话就循环引用了, Result 对象生命周期长于 Task
    Result* result_;
};


/**
 * @brief 线程池支持的模式
 * 
 */
enum class PoolMode {
	/// 固定数量的线程
	MODE_FIXED,	
	/// 线程数量可动态增长
	MODE_CACHED,
};


/**
 * @brief 线程类型
 * 
 */
class Thread {
public:
    /// 线程函数对象类型
    using ThreadFunc = std::function<void(int)>;
    /// 线程构造
    Thread(ThreadFunc func);
    /// 线程析构
    ~Thread();
    /// 启动线程
    void start();
    /// 获取线程 id
    int getId()const;
private:
    ThreadFunc func_;
    static int generateId_;
    /// 保存线程 id
    int threadId_;
};


/**
 * @brief 线程池类型
 * @example
 * ThreadPool pool
 * pool.start();
 *
 * class MyTask : public Task {
 *  public:
 *      void run() {// 线程代码 ...}
 * }
 *
 * pool.sumbitTask(std::make_shared<MyTask>());
 *
 */
class ThreadPool {
public:
	/// 线程池构造
	ThreadPool();

	/// 线程池析构
	~ThreadPool();

	/// 开启线程池
	void start(int initThreadSize = 4);

	/// 设置线程池模式
	void setMode(PoolMode mode);

	/// 设置任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshold);

    /// 设置线程池 cached 模式下线程阈值
    void setThreadSizeThreshHold(int threshold);

	/// 设置初始的线程数量
    void setInitThreadSize(int size);

	/// 给线程池提交任务
	Result submitTask(std::shared_ptr<Task> sp);


	/// 禁止拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
    /// 定义线程函数
    void threadFunc(int threadid);
    /// 检查 pool 运行状态
    bool checkRunningState() const;
private:
	/// 线程列表
    ///	std::vector<Thread*> threads_;
    /// vector 会自动调用存的东西的析构，但是存裸指针的话还要手动析构
    /// 使用 unique_ptr 来管理指针的析构
//    std::vector<std::unique_ptr<Thread>> threads_;
    /// 添加 threadId 来索引 thread 对象
    std::unordered_map<int,std::unique_ptr<Thread>> threads_;

    /// 初始线程数量
    int initThreadSize_;
    /// 记录当前线程池里面的线程数量，不用 threads_.size() 因为不是线程安全的
    std::atomic_int curThreadSize_;
    /// 线程数量上限阈值 防止 cached模式 无限增长
    int threadSizeThreshold_;



    // std::queue<Task*> 用户可能传一个临时任务对象
	// 出了提交任务的语句对象就析构了，拿了已经析构的对象就没用了
	// 使用智能指针保持拉长对象声明周期，自动释放资源
	
	/// 任务队列
	std::queue<std::shared_ptr<Task> > taskQue_;
		
	/// 任务数量 被多线程加减，原子类型
	std::atomic_uint taskSize_; 

	/// 任务队列数量上限的阈值
	int taskQueMaxSizeThreshold_;

    /// 记录空闲线程数量
    std::atomic_int idleThreadSize_;



	/// 保证任务队列线程安全
	std::mutex taskQueMtx_;

	// 生产者消费者模式要两个条件变量

	/// 任务队列不满
	std::condition_variable notFull_;

	/// 任务队列不空
	std::condition_variable notEmpty_;

    /// 等待线程资源全部回收
    std::condition_variable exitCond_;


	/// 当前线程池模式
	PoolMode poolMode_;

    /// 表示线程池是否正在运行, 可能多个线程用到
    /// 设置模式之前要确保没有在运行
    std::atomic_bool isPoolRunning_;

};

#endif