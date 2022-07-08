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
        std::cout << "tid:"<< std::this_thread::get_id()
            << "begin!" << std::endl;

//        std::this_thread::sleep_for(std::chrono::seconds(2));
        ULong sum = 0;
        for(int i = begin_; i < end_; ++i) {
            sum += i;
        }

        std::cout << "tid:"<< std::this_thread::get_id()
                  << "end!" << std::endl;

        return sum;
    }

private:
    int begin_;
    int end_;
};

int main() {
    ThreadPool pool;
    pool.start(4);
//    pool.setTaskQueMaxThreshHold(3);    // 测试提交任务失败

    // 任务没完成前用户获取返回值应该是阻塞的

//    Result res = pool.submitTask(std::make_shared<MyTask>(1,10));
//    int sum = res.get().cast_<int>();  // get 返回一个 Any 类型 怎么转成一个具体类型？
//
//    // 随着 task 执行完，task对象没了，依赖于 task对象的 Result 对象也没了
//    Result res2 = pool.submitTask(std::make_shared<MyTask>(1,5));
//    int sum2 = res.get().cast_<long>();  // 测试异常
//
//    pool.submitTask(std::make_shared<MyTask>(1,10));
//    pool.submitTask(std::make_shared<MyTask>(1,10));
//    pool.submitTask(std::make_shared<MyTask>(1,10));
//    pool.submitTask(std::make_shared<MyTask>(1,10));
//    pool.submitTask(std::make_shared<MyTask>(1,10));l

    Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001,200000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001,300000000));

    ULong sum1 = res1.get().cast_<ULong>();
    ULong sum2 = res2.get().cast_<ULong>();
    ULong sum3 = res3.get().cast_<ULong>();

    // Master - Slave 线程模型
    // Master 线程来分解任务，然后各个Salve线程分配任务
    // 等待各个 Slave 线程执行完任务，返回结果
    // Master 线程合并各个任务结果输出
    std::cout << (sum1 + sum2 + sum3) << std::endl;
    // 因为线程设置了分离，所以测试的时候主线程睡一会防止主程序结束太快了 换成getchar 也可以
//    std::this_thread::sleep_for(std::chrono::seconds(5));
    getchar();
return 0;
}