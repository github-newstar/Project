#include"threadpool.h"
#include <chrono>
#include <cstdio>
#include<iostream>
#include <memory>
#include <thread>

class MyTask: public Task{
public:
    MyTask(int begin, int end):
        begin_(begin)
        ,end_(end)  {}

    Any run() override{
        std::this_thread::sleep_for(std::chrono::seconds(5));
        int sum = 0;
        for(int i = begin_; i < end_; i++){
            sum+=i;
        }
        return sum;
    }
private:
    int begin_;
    int end_;
};
int main(int argc, char** argv){
    {
    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(2, 100000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(3, 100000));
    Result res4 = pool.submitTask(std::make_shared<MyTask>(5, 100000));
    pool.submitTask(std::make_shared<MyTask>(4, 100000));
    pool.submitTask(std::make_shared<MyTask>(4, 100000));
    pool.submitTask(std::make_shared<MyTask>(4, 100000));
    pool.submitTask(std::make_shared<MyTask>(4, 100000));

    Result res5 = pool.submitTask(std::make_shared<MyTask>(5, 100000));
    Result res6 = pool.submitTask(std::make_shared<MyTask>(6, 100000));
    Result res7 = pool.submitTask(std::make_shared<MyTask>(7, 100000));
    int sum = 0;
    std::cout<<"sum = "<<sum<<std::endl;
    }
    getchar();
}
