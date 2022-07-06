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
 * @brief 任务抽象积累
 * @note 用户可以自定义任务数据类型，从 Task 继承
 *			重写 run 方法，实现自定义任务处理
 * 
 */
class Task {
public:
	virtual void run() = 0;		
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
    using ThreadFunc = std::function<void()>;

    /// 线程构造
    Thread(ThreadFunc func);
    /// 线程析构
    ~Thread();
    /// 启动线程
    void start();

private:
    ThreadFunc func_;
};


/**
 * @brief 线程池类型
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

	/// 设置初始的线程数量
	void setInitThreadSize(int size);

	/// 给线程池提交任务
	void submitTask(std::shared_ptr<Task> sp);


	/// 禁止拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
    /// 定义线程函数
    void threadFunc();

private:
	/// 线程列表
    ///	std::vector<Thread*> threads_;
    /// vector 会自动调用存的东西的析构，但是存裸指针的话还要手动析构
    /// 使用 unique_ptr 来管理指针的析构
    std::vector<std::unique_ptr<Thread>> threads_;
    /// 初始线程数量
	std::size_t initThreadSize_;	

	
	// std::queue<Task*> 用户可能传一个临时任务对象
	// 出了提交任务的语句对象就析构了，拿了已经析构的对象就没用了
	// 使用智能指针保持拉长对象声明周期，自动释放资源
	
	/// 任务队列
	std::queue<std::shared_ptr<Task> > taskQue_;
		
	/// 任务数量 被多线程加减，原子类型
	std::atomic_uint taskSize_; 

	/// 任务队列数量上限的阈值
	int taskQueMaxSizeThreshold_;

	/// 保证任务队列线程安全
	std::mutex taskQueMtx_;


	// 生产者消费者模式要两个条件变量

	/// 任务队列不满
	std::condition_variable notFull_;

	/// 任务队列不空
	std::condition_variable notEmpty_;

	/// 当前线程池模式
	PoolMode poolMode;
};

#endif