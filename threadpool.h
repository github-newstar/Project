#ifndef THREADPOOL_H
#define THEADPOOL_H
#include <algorithm>
#include <thread>
#include<unordered_map>
#include<functional>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>

//achieve semaphore
class Semaphore{
public:
    Semaphore(int limit = 0): resLimit_(limit){}
    ~Semaphore() = default;
    //get one semaphore
    void wait(){
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [&]()->bool{ return resLimit_ > 0;});
        resLimit_--;
    }

    //add one semaphore
    void post(){
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }

private:
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};
//class task base
class Any{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&& other) noexcept : base_(std::move(other.base_)) {}
    Any& operator=(Any&& other) noexcept{
        if(this != &other){
            base_ = std::move(other.base_);
        }
        return *this;
    }
    //this construct make any accept anytype
    template<typename T>
    Any(T data):base_(std::make_unique<Driver<T>>(data)){};

    template<typename T>
    T cast_(){
        Driver<T> *ptr = dynamic_cast<Driver<T>*>(base_.get());
        if(ptr == nullptr){
            throw "type is unmatch";
        }
        return ptr->data_;
    }
private:
    class Base{
        public:
            virtual ~Base() = default;
    };

    template<typename  T>
    class Driver:public Base{
        public:
            Driver(T data): data_(data){}
            T data_;
    };
private:
    std::unique_ptr<Base> base_;

};
class Task;
class Result{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;
    //setVal   get return_value of task
    void setVal(Any any);
    //get user get return value for task
    Any get();

private:
    Any any_;
    Semaphore sem_;
    std::shared_ptr<Task> task_;
    bool isValid_;
};
class Task{
public:
    Task();
    ~Task() = default;
    void exec();
    void setResult(Result* res);
    virtual Any run() = 0; //the task defined by user
private:
    Result* result_; //live longer than task
};
//pool mode
enum class PoolMode{
    MODE_FIXED,     //thread pool num fixed
    MODE_CACHED,    //thread pool num can dynamic increase
};
//thread class
class Thread{
public:
        using ThreadFunc = std::function<void(int)>;
        //thread consturct 
        Thread(ThreadFunc func);

        //thread delete
        ~Thread();
        //start the thread
        void start();

        int  getThreadId() const;
private:
        static int generateId_;
        ThreadFunc threadFunc_;
        int threadId_;
};
/*
 * exaple:
 * ThreadPool pool;
 * pool.start(4);
 * class Mytask: public Task{
 *      void run() override{}
 * }
 * pool.submitTask(std::make_shared<Mytask>());
 */
//threadpool class
class ThreadPool{
public:
    ThreadPool();
    ~ThreadPool();
    //set threadpool mode
    void setMode(PoolMode mode);

    //set max taskque size
    void setTaskQueMaxThreshHold(int threshhold);

    void setThreadSizeMaxTHreshHold(int threshhold);

    //submit task
    Result submitTask(std::shared_ptr<Task> sp);

    //start threadpool
    void start(int initThreadSize = std::thread::hardware_concurrency());

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
private:
    //define thread handler
    void threadFunc(int threadId);
    bool checkRunningState() const;
private:
    //std::vector<std::unique_ptr<Thread>> threads_;      //thread list
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;//new thread list
    std::size_t initThreadSize_;        //origin thread list size
    int threadSizeMaxThreshHold_;
    std::atomic_int curThreadSize_;
    std::atomic_int  idleThreadSize_;

    std::queue<std::shared_ptr<Task>> taskQue_; //taskqueue,shared_ptr for temp task
    std::atomic_int taskSize_;       //taskque size
    int taskQueMaxThreshHold_;        //max taskque

    std::mutex taskQueMtx_;         //safe guard for taskthread
    std::condition_variable notFull_; // taskque not full
    std::condition_variable notEmpty_; // taskque nott empty
    std::condition_variable exitCond_; //wait for thread join

    std::atomic_bool isRunning_;
    PoolMode poolMode_;
};

#endif // DEBUG
