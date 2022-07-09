//
// Created by zsl on 7/6/22.
//
#include "threadpool.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using ULong = unsigned long long;

class MyTask : public Task {
public:
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end)
    {}

    // 如何设计 run 函数的返回值，可以表示任意的类型
    // Java Python Object 是所有其他类型的基类 C++17 Any 类型
    Any run() {
        std::cout << "tid:"<< std::this_thread::get_id() << "begin!" << std::endl;
        ULong sum = 0;
        for(int i = begin_; i < end_; ++i) {
            sum += i;
        }

//        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::cout << "tid:"<< std::this_thread::get_id()
                  << "end!" << std::endl;

        return sum;
    }

private:
    int begin_;
    int end_;
};

int main() {
    std::cout << "测试死锁" << std::endl;
    {
        ThreadPool pool;
        pool.start(4);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));

        ULong sum1 = res1.get().cast_<ULong>();
        std::cout << sum1 << std::endl;
    }
    std::cout << "main() over" << std::endl;
    // 防止没打印完就结束了
    std::this_thread::sleep_for(std::chrono::seconds(5));


#if 0
    // 测试析构后回收线程资源
    {
        ThreadPool pool;
        // 用户设置线程池模式
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);
        // 测试提交任务失败
        // pool.setTaskQueMaxThreshHold(3);

        Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001,200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001,300000000));
        Result res4 = pool.submitTask(std::make_shared<MyTask>(300000001,400000000));

        // 6个任务，4个线程，每个任务 3s 所以线程池动态创建了 2 个出来
        pool.submitTask(std::make_shared<MyTask>(300000001,400000000));
        pool.submitTask(std::make_shared<MyTask>(300000001,400000000));

        ULong sum1 = res1.get().cast_<ULong>();
        ULong sum2 = res2.get().cast_<ULong>();
        ULong sum3 = res3.get().cast_<ULong>();
        ULong sum4 = res4.get().cast_<ULong>();

        // 睡 70s 自动回收 2 个多余线程
//        std::this_thread::sleep_for(std::chrono::seconds(70));

        // Master - Slave 线程模型
        // Master 线程来分解任务，然后各个Salve线程分配任务
        // 等待各个 Slave 线程执行完任务，返回结果
        // Master 线程合并各个任务结果输出
        std::cout << (sum1 + sum2 + sum3 + sum4) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(6));
    }
    getchar();
#endif

    return 0;
}