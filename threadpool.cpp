#include"threadpool.h"
#include <chrono>
#include <condition_variable>
#include<iostream>
#include <memory>
#include <mutex>
#include<thread>
#include<functional>

int const TASK_MAX_THRESHHOLD = 1024;
int const THREAD_MAX_THRESHHOLD = 200;
int const THREAD_IMAX_IDLE_TIME = 10;
//define thread 
Thread::Thread(ThreadFunc func)
    :threadFunc_(func)
     ,threadId_(generateId_++)
{}
Thread::~Thread(){
}
//achievement of thread
void Thread::start(){
    std::thread t(threadFunc_, threadId_);
    t.detach();
}

int Thread::generateId_ = 0;
//get threadId
int Thread::getThreadId() const{
    return threadId_;
}
//create threadpool
ThreadPool::ThreadPool()
    :initThreadSize_(4)
     ,taskSize_(0)
     ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
     ,poolMode_(PoolMode::MODE_FIXED)
     ,isRunning_(false)
     ,idleThreadSize_(0)
     ,threadSizeMaxThreshHold_(THREAD_MAX_THRESHHOLD)
     ,curThreadSize_(0)
{}

//delete threadpoll
ThreadPool::~ThreadPool(){
    this->isRunning_ = false;
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]{
            return curThreadSize_ == 0;
            });
}

//set threadpool mode
void ThreadPool::setMode(PoolMode mode){
    if(checkRunningState()){
      return;
    }
    poolMode_ = mode;
}

//set max taskque size
void ThreadPool::setTaskQueMaxThreshHold(int threshhold){
    if(checkRunningState()){
      return;
    }
    taskQueMaxThreshHold_ = threshhold;
};
//set max theadsize 
void ThreadPool::setThreadSizeMaxTHreshHold(int threshhold){
    if(checkRunningState()){
        return;
    }
    threadSizeMaxThreshHold_ = threshhold;
}

//submit task
Result ThreadPool::submitTask(std::shared_ptr<Task> sp){
    //get the locker
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    //thread commucate and wait for taskque notfull
    if(notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool{
            return taskQue_.size() < (size_t)taskQueMaxThreshHold_;}) == false){
        std::cerr<<"task queue is full, submitTask failed"<<std::endl;
        return Result(sp, false);
    }
    //if notfull,add task
    taskQue_.emplace(sp);
    taskSize_++;
    
    //notify notempty
    notEmpty_.notify_all();
    //if cached, them dymatic adopt threadpool size
    if(poolMode_ == PoolMode::MODE_CACHED 
            && taskSize_ > idleThreadSize_
            && curThreadSize_ < threadSizeMaxThreshHold_){
        std::cout<<"has created new thread"<<std::endl;
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
        threads_[threadId]->start();
        idleThreadSize_++;
        curThreadSize_++;
        std::cout<<"create new thread!"<<std::endl;
    }
    return Result(sp);
}

//start threadpool
void ThreadPool::start(int initThreadSize){
    //change the running statues
    isRunning_ = true;
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize_;
    //create all init threads
    for(int i = 0; i < initThreadSize_; i++){
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
    }
    //start all threads
    for(int i = 0; i < initThreadSize_; i++){
        threads_[i]->start();
        idleThreadSize_++;
    }
}
//define threadHandler
void ThreadPool::threadFunc(int threadId){
    auto lastTime = std::chrono::high_resolution_clock().now();
    while(1){
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout<<std::this_thread::get_id()<<"try to get task"<<std::endl;
            while ( taskSize_ == 0) {
                if(!isRunning_){

                    threads_.erase(threadId);
                    curThreadSize_--;
                    exitCond_.notify_all();
                    std::cout<<"thread id:"<<std::this_thread::get_id()<<"exit"<<std::endl;
                    return;
                }
                if(poolMode_ == PoolMode::MODE_CACHED){
                    if(std::cv_status::timeout ==
                            notEmpty_.wait_for(lock, std::chrono::seconds(1))){
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if(dur.count() >= THREAD_IMAX_IDLE_TIME &&
                                curThreadSize_ > initThreadSize_){
                            //start cycle current thread
                            //change val about thread
                            this->curThreadSize_--;
                            this->idleThreadSize_--;
                            //delete this thread from threads list
                            threads_.erase(threadId);
                            std::cout<<"thread has deleted, thread id :"<<std::this_thread::get_id()<<std::endl;
                            return;
                        }
                    }
                }
                else{
                    notEmpty_.wait(lock);
                }
               // if(!isRunning_){
               //     threads_.erase(threadId);
               //     std::cout<<"thread id:"<<std::this_thread::get_id()<<"exit"<<std::endl;
               //     curThreadSize_--;
               //     exitCond_.notify_all();
               //     return;
               // }
            }
           // if(!isRunning_){
           //     break;
           // }
            //idleThreadSize_ decrease
            idleThreadSize_--;
            std::cout<<"thread id:"<<std::this_thread::get_id()<<"get task success"<<std::endl;
            //get onetask
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            //notify other thread to run task
            if(taskSize_ > 0){
                notEmpty_.notify_all();
            }
            //notify can submitTask
            notFull_.notify_all();
        }
        //runtask
        if(task != nullptr)
            task->exec();
        lastTime = std::chrono::high_resolution_clock().now();
        idleThreadSize_++;
    }
}
//define is pool running
bool ThreadPool::checkRunningState() const{
  return this->isRunning_;
}
//achievement of result
Result::Result(std::shared_ptr<Task> task, bool isValid) : task_(task), isValid_(isValid) {
    task_->setResult(this);
}
void Result::setVal(Any any){
    this->any_ = std::move(any);
    sem_.post();
}

Any Result::get(){
    if(task_ == nullptr){
        return "";
    }
    sem_.wait();
    return std::move(any_);
}
//define Task
Task::Task() : result_(nullptr){}

void Task::setResult(Result* res){
    result_ = res;
}
void Task::exec(){
    if(result_ != nullptr){
        result_->setVal(run());
    }
}
