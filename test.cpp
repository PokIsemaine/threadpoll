//
// Created by zsl on 7/6/22.
//
#include "threadpool.hpp"
#include <iostream>
#include <chrono>
#include <thread>

class MyTask : public Task {
public:
    void run() {
        std::cout << "tid:"<< std::this_thread::get_id()
            << "begin!" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "tid:"<< std::this_thread::get_id()
                  << "end!" << std::endl;
    }
};

int main() {
    ThreadPool pool;
    pool.start(4);
    pool.setTaskQueMaxThreshHold(3);    // 测试提交任务失败

    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());

    // 因为线程设置了分离，所以测试的时候主线程睡一会防止主程序结束太快了 换成getchar 也可以
//    std::this_thread::sleep_for(std::chrono::seconds(5));
    getchar();
return 0;
}