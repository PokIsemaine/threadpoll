//
// Created by zsl on 7/6/22.
//
#include "threadpool.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    ThreadPool pool;
    pool.start();

    // 因为线程设置了分离，所以测试的时候主线程睡一会防止主程序结束太快了
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}