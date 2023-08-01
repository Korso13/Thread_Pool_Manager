#pragma once

#include <iostream>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <map>
#include <set>
#include <deque>
#include <mutex>

//wrapper class for our tasks
class AsyncTask
{
private:
	
	AsyncTask() = delete;

	uint64_t TaskID;

	std::function<void()>& TaskToRun;

	bool bNotifyMainThreadOnFinish = false;

	bool bIsComplete = false;

public:

	AsyncTask(std::function<void()>& _InTask, uint64_t _ThisTaskID) : TaskToRun(_InTask), TaskID(_ThisTaskID) {	};

	~AsyncTask() {};

	uint64_t GetTaskID() const { return TaskID; }

	//starts asyncronous task
	void StartTask() {TaskToRun();};

	void SetNotifyMainThreadOnFinish() { bNotifyMainThreadOnFinish = true; }

	bool ShouldNotifyMainThreadOnFinish() const { return bNotifyMainThreadOnFinish; }

	bool IsCompleted() const { return bIsComplete; };

	void SetCompleted() { bIsComplete = true; }
};

static class ThreadPool* ThreadPoolInst = nullptr;
static std::thread MainPoolManagerThread;

//main class managering threads
class ThreadPool
{

public:

	static ThreadPool* Instance(size_t _MaxThreads)
	{
		if (!ThreadPoolInst)
		{
			ThreadPoolInst = new ThreadPool(_MaxThreads);
			std::thread MainPoolManagerThread(&ThreadPool::LaunchThreadPool, ThreadPoolInst);
			MainPoolManagerThread.detach();
		}
		else
		{
			std::cerr << "ThreadPool already instanced with " << ThreadPoolInst->MaxThreads << " threads!" << std::endl;
		}

		return ThreadPoolInst;
	}

	//launches class's main thread; Initializes all Task Threads(aka ThreadWorker())
	void LaunchThreadPool();

	//adds a new function to the pool of tasks;
	//does not accept functions with variable amount of random arguements or returns other than void at the moment;
	//functions with arguements can be passed via std::bind(FunctionName, Arguements...) asssigned to a std::function<void()> variable;
	uint64_t AddTask(std::function<void()>& _InFunc);

	//2 member functions that lock calling thread until selected tasks are complete
	void wait(uint64_t _TaskID);

	void wait_all();

protected:

	ThreadPool(size_t _MaxThreads);

	//deleted default constructor
	ThreadPool() = delete;

	//predicate for use in Wait() and Wait_all() methods; passed to condition variable and called, when it fires a notice
	bool IsMainThreadUnlocked() { return bMainThreadUnlocked; };

	//Task Threads' function working in infinite loop
	void ThreadWorker();

	//in case we need to lock not the class' main thread, but a calling thread, we put this (namely in wait() and wait_all() at the spot, where we lock teh main thread)s
	void CallingThreadLocker();


private:

	//Mutextes and condition variables:
	std::mutex M_MainFunction;

	std::mutex M_CallingThread;

	std::recursive_mutex M_TasksQueue;

	std::recursive_mutex M_ThreadsStatus;

	std::recursive_mutex M_TaskIDs;

	std::condition_variable CV_MainFunction;

	//Data arrays:

	//Map containing all worker threads' statuses. One record per each Task Thread(aka ThreadWorker()); 
	//in std::pair, bool indicates thread's status(working/not working), uint64_t - active task's ID
	std::map<std::thread::id, std::pair<bool, uint64_t>> ThreadsStatus;

	//Tasks' queue
	std::deque<AsyncTask*> TasksQueue; 

	//Tasks' unique IDs and respective TaskWrappers
	std::map<uint64_t, AsyncTask*> TaskIDs; //assures unique TaskIDs

	//Other variables:
	bool bMainThreadUnlocked = true;

	std::atomic<uint64_t> NumberOfLockingTasks = 0;

	size_t MaxThreads = 0;
};
